/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BRAVE_BROWSER_BRAVE_REWARDS_REWARDS_SERVICE_FACTORY_H_
#define BRAVE_BROWSER_BRAVE_REWARDS_REWARDS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace brave_rewards {
class RewardsService;

// Singleton that owns all RewardsService and associates them with
// Profiles.
class RewardsServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static brave_rewards::RewardsService* GetForProfile(Profile* profile);

  static RewardsServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<RewardsServiceFactory>;

  RewardsServiceFactory();
  ~RewardsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(RewardsServiceFactory);
};

}  // namepsace brave_rewards

#endif  // BRAVE_BROWSER_BRAVE_REWARDS_REWARDS_SERVICE_FACTORY_H_
