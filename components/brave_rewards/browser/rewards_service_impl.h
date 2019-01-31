/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_PAYMENTS_PAYMENTS_SERVICE_IMPL_
#define BRAVE_BROWSER_PAYMENTS_PAYMENTS_SERVICE_IMPL_

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "bat/ads/issuers_info.h"
#include "bat/ads/notification_info.h"
#include "bat/ledger/ledger.h"
#include "bat/ledger/wallet_info.h"
#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "base/memory/weak_ptr.h"
#include "bat/confirmations/confirmations_client.h"
#include "bat/ledger/ledger_client.h"
#include "brave/components/services/bat_ledger/public/interfaces/bat_ledger.mojom.h"
#include "brave/components/brave_rewards/browser/rewards_service.h"
#include "brave/components/brave_ads/browser/ads_service.h"
#include "brave/components/brave_ads/browser/ads_service_factory.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/one_shot_event.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "brave/components/brave_rewards/browser/balance_report.h"
#include "brave/components/brave_rewards/browser/content_site.h"
#include "brave/components/brave_rewards/browser/contribution_info.h"
#include "ui/gfx/image/image.h"
#include "brave/components/brave_rewards/browser/publisher_banner.h"
#include "brave/components/brave_rewards/browser/rewards_service_private_observer.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "brave/components/brave_rewards/browser/extension_rewards_service_observer.h"
#endif

namespace base {
class OneShotTimer;
class RepeatingTimer;
class SequencedTaskRunner;
}  // namespace base

namespace ledger {
class Ledger;
class LedgerCallbackHandler;
struct LedgerMediaPublisherInfo;
}  // namespace ledger

namespace leveldb {
class DB;
}  // namespace leveldb

namespace net {
class URLFetcher;
}  // namespace net

class Profile;

namespace brave_rewards {

class PublisherInfoDatabase;
class RewardsNotificationServiceImpl;

using GetProductionCallback = base::Callback<void(bool)>;
using GetReconcileTimeCallback = base::Callback<void(int32_t)>;
using GetShortRetriesCallback = base::Callback<void(bool)>;

class RewardsServiceImpl : public RewardsService,
                            public ledger::LedgerClient,
                            public net::URLFetcherDelegate,
                            public base::SupportsWeakPtr<RewardsServiceImpl> {
 public:
  RewardsServiceImpl(Profile* profile);
  ~RewardsServiceImpl() override;

  // KeyedService:
  void Shutdown() override;

  void Init();
  void StartLedger();
  void CreateWallet() override;
  void FetchWalletProperties() override;
  void FetchGrants(const std::string& lang, const std::string& paymentId) override;
  void GetGrantCaptcha() override;
  void SolveGrantCaptcha(const std::string& solution) const override;
  void GetWalletPassphrase(
      const GetWalletPassphraseCallback& callback) override;
  void GetNumExcludedSites(
      const GetNumExcludedSitesCallback& callback) override;
  void RecoverWallet(const std::string passPhrase) const override;
  void GetContentSiteList(
      uint32_t start,
      uint32_t limit,
      uint64_t min_visit_time,
      uint64_t reconcile_stamp,
      bool allow_non_verified,
      const GetContentSiteListCallback& callback) override;
  void OnGetContentSiteList(
    const GetContentSiteListCallback& callback,
    const ledger::PublisherInfoList& list,
    uint32_t next_record);
  void OnLoad(SessionID tab_id, const GURL& url) override;
  void OnUnload(SessionID tab_id) override;
  void OnShow(SessionID tab_id) override;
  void OnHide(SessionID tab_id) override;
  void OnForeground(SessionID tab_id) override;
  void OnBackground(SessionID tab_id) override;
  void OnMediaStart(SessionID tab_id) override;
  void OnMediaStop(SessionID tab_id) override;
  void OnXHRLoad(SessionID tab_id,
                 const GURL& url,
                 const GURL& first_party_url,
                 const GURL& referrer) override;
  void OnPostData(SessionID tab_id,
                  const GURL& url,
                  const GURL& first_party_url,
                  const GURL& referrer,
                  const std::string& post_data) override;
  std::string URIEncode(const std::string& value) override;
  void GetReconcileStamp(const GetReconcileStampCallback& callback) override;
  void GetAddresses(const GetAddressesCallback& callback) override;
  void GetAutoContribute(
      const GetAutoContributeCallback& callback) override;
  void GetPublisherMinVisitTime(
      const GetPublisherMinVisitTimeCallback& callback) override;
  void GetPublisherMinVisits(
      const GetPublisherMinVisitsCallback& callback) override;
  void GetPublisherAllowNonVerified(
      const GetPublisherAllowNonVerifiedCallback& callback) override;
  void GetPublisherAllowVideos(
      const GetPublisherAllowVideosCallback& callback) override;
  void LoadPublisherInfo(
      const std::string& publisher_key,
      ledger::PublisherInfoCallback callback) override;
  void LoadMediaPublisherInfo(
      const std::string& media_key,
      ledger::PublisherInfoCallback callback) override;
  void SaveMediaPublisherInfo(const std::string& media_key, const std::string& publisher_id) override;
  void ExcludePublisher(const std::string publisherKey) const override;
  void RestorePublishers() override;
  void GetAllBalanceReports(
      const GetAllBalanceReportsCallback& callback) override;
  void GetCurrentBalanceReport() override;
  void IsWalletCreated(const IsWalletCreatedCallback& callback) override;
  void GetPublisherActivityFromUrl(
      uint64_t windowId,
      const std::string& url,
      const std::string& favicon_url,
      const std::string& publisher_blob) override;
  void GetContributionAmount(const GetContributionAmountCallback& callback) override;
  void GetPublisherBanner(const std::string& publisher_id) override;
  void OnPublisherBanner(std::unique_ptr<ledger::PublisherBanner> banner);
  void RemoveRecurring(const std::string& publisher_key) override;
  void UpdateRecurringDonationsList() override;
  void UpdateTipsList() override;
  void SetContributionAutoInclude(
    const std::string& publisher_key, bool excluded, uint64_t windowId) override;
  RewardsNotificationService* GetNotificationService() const override;
  bool CheckImported() override;
  void SetBackupCompleted() override;

  void HandleFlags(const std::string& options);
  void SetProduction(bool production);
  void GetProduction(const GetProductionCallback& callback);
  void SetReconcileTime(int32_t time);
  void GetReconcileTime(const GetReconcileTimeCallback& callback);
  void SetShortRetries(bool short_retries);
  void GetShortRetries(const GetShortRetriesCallback& callback);

  void OnWalletProperties(ledger::Result result,
                          std::unique_ptr<ledger::WalletInfo> info) override;
  void OnDonate(const std::string& publisher_key, int amount, bool recurring,
      std::unique_ptr<brave_rewards::ContentSite> site) override;
  void GetAutoContributeProps(
      const GetAutoContributePropsCallback& callback) override;
  void GetPendingContributionsTotal(
    const GetPendingContributionsTotalCallback& callback) override;
  void GetRewardsMainEnabled(
    const GetRewardsMainEnabledCallback& callback) const override;

  void SetCatalogIssuers(std::unique_ptr<ads::IssuersInfo> info) override;
  void AdSustained(std::unique_ptr<ads::NotificationInfo> info) override;

  void URLRequest(const std::string& url,
                  const std::vector<std::string>& headers,
                  const std::string& content,
                  const std::string& content_type,
                  const ledger::URL_METHOD method,
                  ledger::URLRequestCallback callback) override;
  void SaveConfirmationsState(const std::string& name,
                              const std::string& value,
                              ledger::OnSaveCallback callback) override;
  void LoadConfirmationsState(const std::string& name,
                              ledger::OnLoadCallback callback) override;
  void ResetConfirmationsState(const std::string& name,
                               ledger::OnResetCallback callback) override;
  uint32_t SetConfirmationsTimer(uint64_t time_offset) override;
  void KillConfirmationsTimer(uint32_t timer_id) override;

  void OnSavedConfirmationsState(ledger::OnSaveCallback callback, bool success);
  void OnLoadedConfirmationsState(ledger::OnLoadCallback callback,
                                  const std::string& value);
  void OnResetConfirmationsState(ledger::OnResetCallback callback,
                                 bool success);

  // Testing methods
  void SetLedgerEnvForTesting();

 private:
  const extensions::OneShotEvent& ready() const { return ready_; }
  void OnLedgerStateSaved(ledger::LedgerCallbackHandler* handler,
                          bool success);
  void OnLedgerStateLoaded(ledger::LedgerCallbackHandler* handler,
                              const std::string& data);
  void LoadNicewareList(ledger::GetNicewareListCallback callback) override;
  void OnPublisherStateSaved(ledger::LedgerCallbackHandler* handler,
                             bool success);
  void OnPublisherStateLoaded(ledger::LedgerCallbackHandler* handler,
                              const std::string& data);
  void TriggerOnWalletInitialized(int result);
  void TriggerOnWalletProperties(int error_code,
                                 std::unique_ptr<ledger::WalletInfo> result);
  void TriggerOnGrant(ledger::Result result, const ledger::Grant& grant);
  void TriggerOnGrantCaptcha(const std::string& image, const std::string& hint);
  void TriggerOnRecoverWallet(ledger::Result result,
                              double balance,
                              const std::vector<ledger::Grant>& grants);
  void TriggerOnGrantFinish(ledger::Result result, const ledger::Grant& grant);
  void TriggerOnRewardsMainEnabled(bool rewards_main_enabled);
  void OnPublisherInfoSaved(ledger::PublisherInfoCallback callback,
                            std::unique_ptr<ledger::PublisherInfo> info,
                            bool success);
  void OnActivityInfoSaved(ledger::PublisherInfoCallback callback,
                            std::unique_ptr<ledger::PublisherInfo> info,
                            bool success);
  void OnActivityInfoLoaded(ledger::PublisherInfoCallback callback,
                             const ledger::PublisherInfoList list);
  void OnMediaPublisherInfoSaved(bool success);
  void OnPublisherInfoLoaded(ledger::PublisherInfoCallback callback,
                             std::unique_ptr<ledger::PublisherInfo> info);
  void OnMediaPublisherInfoLoaded(ledger::PublisherInfoCallback callback,
                             std::unique_ptr<ledger::PublisherInfo> info);
  void OnPublisherInfoListLoaded(uint32_t start,
                                 uint32_t limit,
                                 ledger::PublisherInfoListCallback callback,
                                 const ledger::PublisherInfoList& list);
  void OnPublishersListSaved(ledger::LedgerCallbackHandler* handler,
                             bool success);
  void OnTimer(uint32_t timer_id);
  void OnConfirmationsTimer(uint32_t timer_id);
  void TriggerOnContentSiteUpdated();
  void OnPublisherListLoaded(ledger::LedgerCallbackHandler* handler,
                             const std::string& data);

  void OnDonate_PublisherInfoSaved(ledger::Result result,
                                   std::unique_ptr<ledger::PublisherInfo> info);
  void OnDonate(const std::string& publisher_key, int amount, bool recurring,
      const ledger::PublisherInfo* publisher_info = NULL) override;
  void OnContributionInfoSaved(const ledger::REWARDS_CATEGORY category, bool success);
  void OnRecurringDonationSaved(bool success);
  void SaveRecurringDonation(const std::string& publisher_key, const int amount);
  void OnRecurringDonationsData(const ledger::PublisherInfoListCallback callback,
                                const ledger::PublisherInfoList list);
  void OnRecurringDonationUpdated(const ledger::PublisherInfoList& list);
  void OnTipsUpdatedData(const ledger::PublisherInfoList list);
  void TipsUpdated();
  void OnRemovedRecurring(ledger::RecurringRemoveCallback callback, bool success);
  void OnRemoveRecurring(const std::string& publisher_key, ledger::RecurringRemoveCallback callback) override;
  void TriggerOnGetCurrentBalanceReport(const ledger::BalanceReportInfo& report);
  void TriggerOnGetPublisherActivityFromUrl(
      ledger::Result result,
      std::unique_ptr<ledger::PublisherInfo> info,
      uint64_t windowId);
  void MaybeShowBackupNotification(uint64_t boot_stamp);
  void MaybeShowAddFundsNotification(uint64_t reconcile_stamp);
  void OnRestorePublishersInternal(ledger::OnRestoreCallback callback,
                                   bool result);

  // ledger::LedgerClient
  std::string GenerateGUID() const override;
  void OnWalletInitialized(ledger::Result result) override;
  void OnGrant(ledger::Result result, const ledger::Grant& grant) override;
  void OnGrantCaptcha(const std::string& image, const std::string& hint) override;
  void OnRecoverWallet(ledger::Result result,
                      double balance,
                      const std::vector<ledger::Grant>& grants) override;
  void OnReconcileComplete(ledger::Result result,
                           const std::string& viewing_id,
                           ledger::REWARDS_CATEGORY category,
                           const std::string& probi) override;
  void OnGrantFinish(ledger::Result result,
                     const ledger::Grant& grant) override;
  void LoadLedgerState(ledger::LedgerCallbackHandler* handler) override;
  void LoadPublisherState(ledger::LedgerCallbackHandler* handler) override;
  void SaveLedgerState(const std::string& ledger_state,
                       ledger::LedgerCallbackHandler* handler) override;
  void SavePublisherState(const std::string& publisher_state,
                          ledger::LedgerCallbackHandler* handler) override;

  void SavePublisherInfo(std::unique_ptr<ledger::PublisherInfo> publisher_info,
                         ledger::PublisherInfoCallback callback) override;
  void SaveActivityInfo(std::unique_ptr<ledger::PublisherInfo> publisher_info,
                        ledger::PublisherInfoCallback callback) override;
  void LoadActivityInfo(ledger::ActivityInfoFilter filter,
                         ledger::PublisherInfoCallback callback) override;
  void LoadPanelPublisherInfo(ledger::ActivityInfoFilter filter,
                              ledger::PublisherInfoCallback callback) override;
  void GetActivityInfoList(
      uint32_t start,
      uint32_t limit,
      ledger::ActivityInfoFilter filter,
      ledger::PublisherInfoListCallback callback) override;
  void SavePublishersList(const std::string& publishers_list,
                          ledger::LedgerCallbackHandler* handler) override;
  void SetTimer(uint64_t time_offset, uint32_t& timer_id) override;
  void LoadPublisherList(ledger::LedgerCallbackHandler* handler) override;

  void LoadURL(const std::string& url,
      const std::vector<std::string>& headers,
      const std::string& content,
      const std::string& contentType,
      const ledger::URL_METHOD& method,
      ledger::LoadURLCallback callback) override;

  void SetRewardsMainEnabled(bool enabled) override;
  void SetPublisherMinVisitTime(uint64_t duration_in_seconds) const override;
  void SetPublisherMinVisits(unsigned int visits) const override;
  void SetPublisherAllowNonVerified(bool allow) const override;
  void SetPublisherAllowVideos(bool allow) const override;
  void SetContributionAmount(double amount) const override;
  void SetUserChangedContribution() const override;
  void SetAutoContribute(bool enabled) const override;
  void OnExcludedSitesChanged(const std::string& publisher_id) override;
  void OnPublisherActivity(ledger::Result result,
                          std::unique_ptr<ledger::PublisherInfo> info,
                          uint64_t windowId) override;
  void FetchFavIcon(const std::string& url,
                    const std::string& favicon_key,
                    ledger::FetchIconCallback callback) override;
  void OnFetchFavIconCompleted(ledger::FetchIconCallback callback,
                          const std::string& favicon_key,
                          const GURL& url,
                          const BitmapFetcherService::RequestId& request_id,
                          const SkBitmap& image);
  void OnSetOnDemandFaviconComplete(const std::string& favicon_url,
                                    ledger::FetchIconCallback callback,
                                    bool success);
  void SaveContributionInfo(const std::string& probi,
                            const int month,
                            const int year,
                            const uint32_t date,
                            const std::string& publisher_key,
                            const ledger::REWARDS_CATEGORY category) override;
  void GetRecurringDonations(ledger::PublisherInfoListCallback callback) override;
  std::unique_ptr<ledger::LogStream> Log(
                     const char* file,
                     int line,
                     const ledger::LogLevel log_level) const override;
  std::unique_ptr<confirmations::LogStream> LogConfirmations(
                     const char* file,
                     int line,
                     const int log_level) const override;
  void OnRestorePublishers(ledger::OnRestoreCallback callback) override;
  void OnPanelPublisherInfoLoaded(
      ledger::PublisherInfoCallback callback,
      std::unique_ptr<ledger::PublisherInfo> publisher_info);

  void SavePendingContribution(
      const ledger::PendingContributionList& list) override;

  void OnSavePendingContribution(ledger::Result result);

  void SetConfirmationsIsReady(const bool is_ready) override;

  // URLFetcherDelegate impl
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  void StartNotificationTimers(bool main_enabled);
  void StopNotificationTimers();
  void OnNotificationTimerFired();

  void MaybeShowNotificationAddFunds();
  bool ShouldShowNotificationAddFunds() const;
  void ShowNotificationAddFunds(bool sufficient);

  // Mojo Proxy methods
  void OnPublisherBannerMojoProxy(const std::string& banner);
  void OnGetAllBalanceReports(
      const GetAllBalanceReportsCallback& callback,
      const base::flat_map<std::string, std::string>& json_reports);
  void OnGetCurrentBalanceReport(
      bool success, const std::string& json_report);
  void OnGetAddresses(
      const GetAddressesCallback& callback,
      const base::flat_map<std::string, std::string>& addresses);
  void OnGetAutoContributeProps(
      const GetAutoContributePropsCallback& callback,
      const std::string& json_props);
  void SetRewardsMainEnabledPref(bool enabled);
  void SetRewardsMainEnabledMigratedPref(bool enabled);

  bool Connected() const;
  void ConnectionClosed();

  Profile* profile_;  // NOT OWNED
  mojo::AssociatedBinding<bat_ledger::mojom::BatLedgerClient>
    bat_ledger_client_binding_;
  bat_ledger::mojom::BatLedgerAssociatedPtr bat_ledger_;
  bat_ledger::mojom::BatLedgerServicePtr bat_ledger_service_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<ExtensionRewardsServiceObserver>
      extension_rewards_service_observer_;
#endif
  const scoped_refptr<base::SequencedTaskRunner> file_task_runner_;
  const base::FilePath ledger_state_path_;
  const base::FilePath publisher_state_path_;
  const base::FilePath publisher_info_db_path_;
  const base::FilePath publisher_list_path_;
  std::unique_ptr<PublisherInfoDatabase> publisher_info_backend_;
  std::unique_ptr<RewardsNotificationServiceImpl> notification_service_;
  base::ObserverList<RewardsServicePrivateObserver> private_observers_;
#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<ExtensionRewardsServiceObserver> private_observer_;
#endif

  extensions::OneShotEvent ready_;
  std::map<const net::URLFetcher*, ledger::LoadURLCallback> fetchers_;
  std::map<uint32_t, std::unique_ptr<base::OneShotTimer>> timers_;
  std::map<uint32_t, std::unique_ptr<base::OneShotTimer>> confirmations_timers_;
  std::vector<std::string> current_media_fetchers_;
  std::vector<BitmapFetcherService::RequestId> request_ids_;
  std::unique_ptr<base::OneShotTimer> notification_startup_timer_;
  std::unique_ptr<base::RepeatingTimer> notification_periodic_timer_;
  const base::FilePath confirmations_base_path_;

  uint32_t next_timer_id_;
  uint32_t next_confirmations_timer_id_;

  DISALLOW_COPY_AND_ASSIGN(RewardsServiceImpl);
};

}  // namespace brave_rewards

#endif  // BRAVE_BROWSER_PAYMENTS_PAYMENTS_SERVICE_IMPL_
