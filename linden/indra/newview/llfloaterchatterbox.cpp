/** 
 * @file llfloaterchatterbox.cpp
 * @author Richard
 * @date 2007-05-08
 * @brief Implementation of the chatterbox integrated conversation ui
 *
 * Copyright (c) 2007-2007, Linden Research, Inc.
 * 
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlife.com/developers/opensource/gplv2
 * 
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at http://secondlife.com/developers/opensource/flossexception
 * 
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 * 
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 */


#include "llviewerprecompiledheaders.h"

#include "llfloaterchatterbox.h"
#include "llvieweruictrlfactory.h"
#include "llfloaterchat.h"
#include "llfloaterfriends.h"
#include "llfloatergroups.h"
#include "llviewercontrol.h"
#include "llimview.h"
#include "llimpanel.h"

//
// LLFloaterMyFriends
//

LLFloaterMyFriends::LLFloaterMyFriends(const LLSD& seed)
{
	mFactoryMap["friends_panel"] = LLCallbackMap(LLFloaterMyFriends::createFriendsPanel, NULL);
	mFactoryMap["groups_panel"] = LLCallbackMap(LLFloaterMyFriends::createGroupsPanel, NULL);
	// do not automatically open singleton floaters (as result of getInstance())
	BOOL no_open = FALSE;
	gUICtrlFactory->buildFloater(this, "floater_my_friends.xml", &getFactoryMap(), no_open);
}

LLFloaterMyFriends::~LLFloaterMyFriends()
{
}

BOOL LLFloaterMyFriends::postBuild()
{
	mTabs = LLUICtrlFactory::getTabContainerByName(this, "friends_and_groups");

	return TRUE;
}


void LLFloaterMyFriends::onClose(bool app_quitting)
{
	setVisible(FALSE);
}

//static 
LLFloaterMyFriends* LLFloaterMyFriends::showInstance(const LLSD& id)
{
	LLFloaterMyFriends* floaterp = LLUIInstanceMgr<LLFloaterMyFriends>::showInstance(id);
	// garbage values in id will be interpreted as 0, or the friends tab
	floaterp->mTabs->selectTab(id);

	return floaterp;
}

//static 
void LLFloaterMyFriends::hideInstance(const LLSD& id)
{
	if(instanceVisible(id))
	{
		LLFloaterChatterBox::hideInstance(LLSD());
	}
}

// is the specified panel currently visible
//static
BOOL LLFloaterMyFriends::instanceVisible(const LLSD& id)
{
	// if singleton not created yet, trivially return false
	if (!findInstance(id)) return FALSE;

	LLFloaterMyFriends* floaterp = getInstance(id);
	return floaterp->isInVisibleChain() && floaterp->mTabs->getCurrentPanelIndex() == id.asInteger();
}

//static
void* LLFloaterMyFriends::createFriendsPanel(void* data)
{
	return new LLPanelFriends();
}

//static
void* LLFloaterMyFriends::createGroupsPanel(void* data)
{
	return new LLPanelGroups();
}

//
// LLFloaterChatterBox
//
LLFloaterChatterBox::LLFloaterChatterBox(const LLSD& seed) :
	mActiveVoiceFloater(NULL)
{
	mAutoResize = FALSE;

	gUICtrlFactory->buildFloater(this, "floater_chatterbox.xml", NULL, FALSE);
	addFloater(LLFloaterMyFriends::getInstance(0), TRUE);
	if (gSavedSettings.getBOOL("ChatHistoryTornOff"))
	{
		LLFloaterChat* floater_chat = LLFloaterChat::getInstance(LLSD());
		// add then remove to set up relationship for re-attach
		addFloater(floater_chat, FALSE);
		removeFloater(floater_chat);
		// reparent to floater view
		gFloaterView->addChild(floater_chat);
	}
	else
	{
		addFloater(LLFloaterChat::getInstance(LLSD()), FALSE);
	}
	mTabContainer->lockTabs();
}

LLFloaterChatterBox::~LLFloaterChatterBox()
{
}

BOOL LLFloaterChatterBox::handleKeyHere(KEY key, MASK mask, BOOL called_from_parent)
{
	if (getEnabled()
		&& mask == MASK_CONTROL)
	{
		if (key == 'W')
		{
			LLFloater* floater = getActiveFloater();
			// is user closeable and is system closeable
			if (floater && floater->canClose())
			{
				if (floater->isCloseable())
				{
					floater->close();
				}
				else
				{
					// close chatterbox window if frontmost tab is reserved, non-closeable tab
					// such as contacts or near me
					close();
				}
			}
			return TRUE;
		}
	}

	return LLMultiFloater::handleKeyHere(key, mask, called_from_parent);
}

void LLFloaterChatterBox::draw()
{
	// clear new im notifications when chatterbox is visible
	if (!isMinimized()) 
	{
		gIMMgr->clearNewIMNotification();
	}
	LLFloater* current_active_floater = getCurrentVoiceFloater();
	// set icon on tab for floater currently associated with active voice channel
	if(mActiveVoiceFloater != current_active_floater)
	{
		// remove image from old floater's tab
		if (mActiveVoiceFloater)
		{
			mTabContainer->setTabImage(mActiveVoiceFloater, "");
		}
	}

	// update image on current active tab
	if (current_active_floater)
	{
		LLColor4 icon_color = LLColor4::white;
		LLVoiceChannel* channelp = LLVoiceChannel::getCurrentVoiceChannel();
		if (channelp)
		{
			if (channelp->isActive())
			{
				icon_color = LLColor4::green;
			}
			else if (channelp->getState() == LLVoiceChannel::STATE_ERROR)
			{
				icon_color = LLColor4::red;
			}
			else // active, but not connected
			{
				icon_color = LLColor4::yellow;
			}
		}
		mTabContainer->setTabImage(current_active_floater, "active_voice_tab.tga", icon_color);
	}

	mActiveVoiceFloater = current_active_floater;

	LLFloater::draw();
}

void LLFloaterChatterBox::onOpen()
{
	gSavedSettings.setBOOL("ShowCommunicate", TRUE);
}

void LLFloaterChatterBox::onClose(bool app_quitting)
{
	setVisible(FALSE);
	gSavedSettings.setBOOL("ShowCommunicate", FALSE);
}

void LLFloaterChatterBox::removeFloater(LLFloater* floaterp)
{
	if (floaterp->getName() == "chat floater")
	{
		// only my friends floater now locked
		mTabContainer->lockTabs(1);
		gSavedSettings.setBOOL("ChatHistoryTornOff", TRUE);
		floaterp->setCanClose(TRUE);
	}
	LLMultiFloater::removeFloater(floaterp);
}

void LLFloaterChatterBox::addFloater(LLFloater* floaterp, 
									BOOL select_added_floater, 
									LLTabContainerCommon::eInsertionPoint insertion_point)
{
	// make sure my friends and chat history both locked when re-attaching chat history
	if (floaterp->getName() == "chat floater")
	{
		// select my friends tab
		mTabContainer->selectFirstTab();
		// add chat history to the right of the my friends tab
		//*TODO: respect select_added_floater so that we don't leave first tab selected
		LLMultiFloater::addFloater(floaterp, select_added_floater, LLTabContainer::RIGHT_OF_CURRENT);
		// make sure first two tabs are now locked
		mTabContainer->lockTabs(2);
		gSavedSettings.setBOOL("ChatHistoryTornOff", FALSE);
		floaterp->setCanClose(FALSE);
	}
	else
	{
		LLMultiFloater::addFloater(floaterp, select_added_floater, insertion_point);
	}

	// make sure active voice icon shows up for new tab
	if (floaterp == mActiveVoiceFloater)
	{
		mTabContainer->setTabImage(floaterp, "active_voice_tab.tga");	
	}
}


//static 
LLFloaterChatterBox* LLFloaterChatterBox::showInstance(const LLSD& seed)
{
	LLFloaterChatterBox* floater = LLUISingleton<LLFloaterChatterBox>::showInstance(seed);

	// if TRUE, show tab for active voice channel, otherwise, just show last tab
	if (seed.asBoolean())
	{
		LLFloater* floater_to_show = getCurrentVoiceFloater();
		if (floater_to_show)
		{
			floater_to_show->open();
		}
		else
		{
			// just open chatterbox if there is no active voice window
			LLUISingleton<LLFloaterChatterBox>::getInstance(seed)->open();
		}
	}
	
	return floater;
}

//static
BOOL LLFloaterChatterBox::instanceVisible(const LLSD &seed)
{
	if (seed.asBoolean())
	{
		LLFloater* floater_to_show = getCurrentVoiceFloater();
		if (floater_to_show)
		{
			return floater_to_show->isInVisibleChain();
		}
	}

	return LLUISingleton<LLFloaterChatterBox>::instanceVisible(seed);
}

//static 
LLFloater* LLFloaterChatterBox::getCurrentVoiceFloater()
{
	if (!LLVoiceClient::voiceEnabled())
	{
		return NULL;
	}
	if (LLVoiceChannelProximal::getInstance() == LLVoiceChannel::getCurrentVoiceChannel())
	{
		// show near me tab if in proximal channel
		return LLFloaterChat::getInstance(LLSD());
	}
	else
	{
		LLFloaterChatterBox* floater = LLFloaterChatterBox::getInstance(LLSD());
		// iterator over all IM tabs (skip friends and near me)
		for (S32 i = 0; i < floater->getFloaterCount(); i++)
		{
			LLPanel* panelp = floater->mTabContainer->getPanelByIndex(i);
			if (panelp->getName() == "im_floater")
			{
				// only LLFloaterIMPanels are called "im_floater"
				LLFloaterIMPanel* im_floaterp = (LLFloaterIMPanel*)panelp;
				if (im_floaterp->getVoiceChannel()  == LLVoiceChannel::getCurrentVoiceChannel())
				{
					return im_floaterp;
				}
			}
		}
	}
	return NULL;
}