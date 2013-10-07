/***
* ==++==
*
* Copyright (c) Microsoft Corporation. All rights reserved. 
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
* http://www.apache.org/licenses/LICENSE-2.0
* 
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
* ==--==
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* Player.xaml.h:  Declaration of the Player class.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include "pch.h"
#include "Player.g.h"

namespace BlackjackClient
{
    public ref class PlayerSpace sealed
    {
    public:
        PlayerSpace() { _init(); }

        void Clear()
        {
            playerBalance->Text = L"";
            playerBet->Text = L"";
            playerName->Text = L"";
            playerInsurance->Text = L"";
            m_cards.clear();
            playerCardGrid->Children->Clear();
        }

        int CardsHeld() { return int(m_cards.size()); }

    private:
        friend ref class PlayingTable;

        void AddCard(Card card);
        void Update(Player player);

        void ShowResult(BJHandResult result);

        void _init();

        std::vector<Card> m_cards;
    };
}
