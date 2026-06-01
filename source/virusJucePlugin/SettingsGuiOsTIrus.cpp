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

		if(!leds || !leds->supportsLogoAnimation())
		{
			juceRmlUi::helper::setVisible(_root, false);
			return;
		}

		// ---- Logo animation -------------------------------------------------
		auto* button = juceRmlUi::helper::findChild(_root, "btEnableLogoAnimations");
		auto* buttonComp = juceRmlUi::helper::findChildT<juceRmlUi::ElemButton>(button, "button");

		buttonComp->setChecked(leds->isLogoAnimationEnabled());

		juceRmlUi::EventListener::AddClick(button, [leds, buttonComp]
		{
			leds->toggleLogoAnimation();
			buttonComp->setChecked(leds->isLogoAnimationEnabled());
		});

#ifdef HAS_CLAP_JUCE_EXTENSIONS
		// ---- CLAP: suggest remote controls page on gesture ------------------
		auto& config = _editor->getProcessor().getConfig();

		auto* clapSection   = juceRmlUi::helper::findChild(_root, "clapSection");
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
#else
		// Hide the CLAP section in non-CLAP builds
		if(auto* clapSection = juceRmlUi::helper::findChild(_root, "clapSection"))
			juceRmlUi::helper::setVisible(clapSection, false);
#endif
	}
}
