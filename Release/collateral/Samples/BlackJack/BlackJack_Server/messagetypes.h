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
* messagetypes.h 
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once
#include "stdafx.h"

enum BJHandResult { HR_None, HR_PlayerBlackJack, HR_PlayerWin, HR_ComputerWin, HR_Push };

enum BJHandState { HR_Empty, HR_BlackJack, HR_Active, HR_Held, HR_Busted };

enum BJPossibleMoves { PM_None = 0x0, PM_Hit = 0x1, PM_DoubleDown = 0x2, PM_Split = 0x4, PM_All = 0x7 };

enum CardSuit { CS_Heart, CS_Diamond, CS_Club, CS_Spade };

enum CardValue { CV_None, CV_Ace, CV_Two, CV_Three, CV_Four, CV_Five, CV_Six, CV_Seven, CV_Eight, CV_Nine, CV_Ten, CV_Jack, CV_Queen, CV_King };

enum BJStatus { ST_PlaceBet, ST_Refresh, ST_YourTurn, ST_None };

#define STATE U("state")
#define BET U("bet")
#define DOUBLE U("double")
#define INSURE U("insure")
#define HIT U("hit")
#define STAY U("stay")
#define REFRESH U("refresh")
#define INSURANCE U("insurance")
#define RESULT U("result")
#define NAME U("Name")
#define BALANCE U("Balance")
#define HAND U("Hand")
#define SUIT U("suit")
#define VALUE U("value")
#define CARDS U("cards")
#define CAPACITY U("Capacity")
#define ID U("Id")
#define PLAYERS U("Players")
#define DEALER U("DEALER")
#define DATA U("Data")
#define STATUS U("Status")
#define REQUEST U("request")
#define AMOUNT U("amount")
#define QUERY_NAME U("name")

struct Card
{
    CardSuit suit;
    CardValue value;

    static Card FromJSON(const web::json::value & object)
    {
        Card result;
        result.suit = (CardSuit)(int)object[SUIT].as_double();
        result.value = (CardValue)(int)object[VALUE].as_double();
        return result;
    }

    web::json::value AsJSON() const 
    {
        web::json::value result = web::json::value::object();
        result[SUIT] = web::json::value::number(suit);
        result[VALUE] = web::json::value::number(value);
        return result;
    }
};

struct NumericHandValues
{
    int low;
    int high;

    int Best() { return (high < 22) ? high : low; }
};

struct BJHand
{
    bool revealBoth;

    std::vector<Card> cards;
    double bet;
    double insurance;
    BJHandState state;
    BJHandResult result;

    BJHand() : state(HR_Empty), result(HR_None), bet(0.0), insurance(0), revealBoth(true) {}

    void Clear() { cards.clear(); state = HR_Empty; result = HR_None; insurance = 0.0; }

    void AddCard(Card card)
    { 
        cards.push_back(card); 
        NumericHandValues value = GetNumericValues();

        if ( cards.size() == 2 && value.high == 21 )
        {
            state = HR_BlackJack;
        }
        else if ( value.low > 21 )
        {
            state = HR_Busted;
        }
        else
        {
            state = HR_Active;
        }
    }

    NumericHandValues GetNumericValues()
    {
        NumericHandValues result;
        result.low = 0;
        result.low = 0;

        bool hasAces = false;

        for (auto iter = cards.begin(); iter != cards.end(); ++iter)
        {
            if ( iter->value == CV_Ace ) hasAces = true;

            result.low += std::min((int)iter->value, 10);
        }
        result.high = hasAces ? result.low + 10 : result.low;
        return result;
    }

    static BJHand FromJSON(const web::json::value &object)
    {
        BJHand result;

        web::json::value cards = object[CARDS];

        for (auto iter = cards.begin(); iter != cards.end(); ++iter)
        {
            if ( !iter->second.is_null() )
            {
                Card card;
                card = Card::FromJSON(iter->second);
                result.cards.push_back(card);
            }
        }

        result.state     = (BJHandState)(int)object[STATE].as_double();
        result.bet       = object[BET].as_double();
        result.insurance = object[INSURANCE].as_double();
        result.result    = (BJHandResult)(int)object[RESULT].as_double();
        return result;
    }

    web::json::value AsJSON() const 
    {
        web::json::value result = web::json::value::object();
        result[STATE] = web::json::value::number(state);
        result[RESULT] = web::json::value::number(this->result);
        result[BET] = web::json::value::number(bet);
        result[INSURANCE] = web::json::value::number(insurance);

        web::json::value jCards = web::json::value::array(cards.size());

        if ( revealBoth )
        {
            int idx = 0;
            for (auto iter = cards.begin(); iter != cards.end(); ++iter)
            {
                jCards[idx++] = iter->AsJSON();
            }
        }
        else
        {
            int idx = 0;
            for (auto iter = cards.begin(); iter != cards.end();)
            {
                jCards[idx++] = iter->AsJSON();
                break;
            }
        }
        result[CARDS] = jCards;
        return result;
    }
};

struct Player
{
    utility::string_t Name;
    BJHand Hand;
    double Balance;

    Player() {}
    Player(const utility::string_t &name) : Name(name), Balance(1000.0) {}

    static Player FromJSON(const web::json::value &object)
    {
        Player result(U(""));

        const web::json::value &name = object[NAME];
        const web::json::value &balance = object[BALANCE];
        const web::json::value &hand = object[HAND];

        result.Name = name.as_string();
        result.Balance = balance.as_double();
        result.Hand = BJHand::FromJSON(hand);
        return result;
    }

    web::json::value AsJSON() const 
    {
        web::json::value result = web::json::value::object();
        result[NAME] = web::json::value::string(Name);
        result[BALANCE] = web::json::value::number(Balance);
        result[HAND] = Hand.AsJSON();
        return result;
    }
};

struct BJTable
{
    int Id;
    size_t Capacity;
    std::vector<Player> Players;

    BJTable() : Capacity(0) {}
    BJTable(int id, size_t capacity) : Id(id), Capacity(capacity) { Players.push_back(Player(DEALER)); }

    static BJTable FromJSON(const web::json::value &object)
    {
        BJTable result;
        result.Id = (int)object[ID].as_double();
        result.Capacity = (size_t)object[CAPACITY].as_double();

        web::json::value players = object[PLAYERS];
        int i = 0;

        for (auto iter = players.begin(); iter != players.end(); ++iter, i++)
        {
            result.Players.push_back(Player::FromJSON(iter->second));
        }

        return result;
    }

    web::json::value AsJSON() const 
    {
        web::json::value result = web::json::value::object();
        result[ID] = web::json::value::number((double)Id);
        result[CAPACITY] = web::json::value::number((double)Capacity);

        web::json::value jPlayers = web::json::value::array(Players.size());

        size_t idx = 0;
        for (auto iter = Players.begin(); iter != Players.end(); ++iter)
        {
            jPlayers[idx++] = iter->AsJSON();
        }
        result[PLAYERS] = jPlayers;
        return result;
    }
};

struct BJPutResponse
{
    BJStatus    Status;
    web::json::value Data;

    BJPutResponse() {}
    BJPutResponse(BJStatus status, web::json::value data) : Status(status), Data(data) { }

    static BJPutResponse FromJSON(web::json::value object)
    {
        return BJPutResponse((BJStatus)(int)object[STATUS].as_double(), object[DATA]);
    }

    web::json::value AsJSON() const 
    {
        web::json::value result = web::json::value::object();
        result[STATUS] = web::json::value::number((double)Status);
        result[DATA] = Data;
        return result;
    }
};

inline web::json::value TablesAsJSON(const utility::string_t &name, const std::map<utility::string_t, std::shared_ptr<BJTable>> &tables)
{
    web::json::value result = web::json::value::array();

    size_t idx = 0;
    for (auto tbl = tables.begin(); tbl != tables.end(); tbl++)
    {
        result[idx++] = tbl->second->AsJSON();
    }
    return result;
}

