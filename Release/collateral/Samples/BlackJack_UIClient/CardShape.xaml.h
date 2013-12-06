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
* CardShape.xaml.h - Declaration of the CardShape class
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#pragma once

#include "pch.h"
#include "CardShape.g.h"

#define CardWidth 71
#define CardHeight 96
#define CardWidthRect 72
#define CardHeightRect 97

#define SCALE_FACTOR 1.80

namespace BlackjackClient
{
    public ref class CardShape sealed
    {
    public:
        CardShape();
        virtual ~CardShape();

    private:
        friend ref class PlayingTable;
        friend ref class PlayerSpace;

        void adjust();

        int _suit;
        int _value;
        Platform::Boolean _visible;
    };
}
