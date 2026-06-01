#include "SettingsGuiOsTIrus.h"

#include "VirusEditor.h"
#include "VirusProcessor.h"

#include "juceRmlUi/rmlElemButton.h"
#include "juceRmlUi/rmlEventListener.h"
#include "juceRmlUi/rmlHelper.h"

namespace genericVirusUI
{
	SettingsGuiOsTIrus::SettingsGuiOsTIrus(const VirusEditor* _editor, Rml::Element* _root)
	{
		auto* leds = _editor->getLeds().get();

		// ---- Logo animation -------------------------------------------------
		// Hide only the animation section when the skin has no logo element;
		// other sections (CLAP, future) are unaffected.
		if(!leds || !leds->supportsLogoAnimation())
		{
			if(auto* animSection = juceRmlUi::helper::findChild(_root, "animationSection"))
				juceRmlUi::helper::setVisible(animSection, false);
		}
		else
		{
			auto* button     = juceRmlUi::helper::findChild(_root, "btEnableLogoAnimations");
			auto* buttonComp = juceRmlUi::helper::findChildT<juceRmlUi::ElemButton>(button, "button");

			buttonComp->setChecked(leds->isLogoAnimationEnabled());

			juceRmlUi::EventListener::AddClick(button, [leds, buttonComp]
			{
				leds->toggleLogoAnimation();
				buttonComp->setChecked(leds->isLogoAnimationEnabled());
			});
		}

		// ---- CLAP: suggest remote controls page on gesture ------------------
		// The section is always shown; the config key is only acted upon by
		// OsTIrusProcessor when running as a CLAP plugin.
		auto& config        = _editor->getProcessor().getConfig();
		auto* btSuggest     = juceRmlUi::helper::findChild(_root, "btClapSuggestPage");
		auto* btSuggestComp = juceRmlUi::helper::findChildT<juceRmlUi::ElemButton>(btSuggest, "button");

		btSuggestComp->setChecked(config.getBoolValue(virus::VirusProcessor::g_clapSuggestPageKey, true));

		juceRmlUi::EventListener::AddClick(btSuggest, [&config, btSuggestComp]
		{
			const bool newValue = !config.getBoolValue(virus::VirusProcessor::g_clapSuggestPageKey, true);
			config.setValue(virus::VirusProcessor::g_clapSuggestPageKey, newValue);
			config.saveIfNeeded();
			btSuggestComp->setChecked(newValue);
		});
	}
}
