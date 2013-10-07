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
* CardShape.xaml.cpp - Implementation of the CardShape class
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/

#include "pch.h"
#include "CardShape.xaml.h"

using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Media;
using namespace BlackjackClient;

CardShape::CardShape() : _suit(CS_Heart), _value(CV_Ace), _visible(true)
{
    InitializeComponent();
}

void CardShape::adjust()
{
    //Adjust the clipping of the cards image to reflect the current card
    double x = 0;
    double y = 0;

    if (_visible)
    {
        x = (double)(int)(_value-1);
        y = (double)(int)_suit;
    }
    else
    {
        //Show back of the card
        x = 0;
        y = 4;
    }

    RectangleGeometry^ clip = imgCard->Clip;
    Windows::Foundation::Rect rect = clip->Rect;
    rect.X = float(x * CardWidthRect);
    rect.Y = float(y * CardHeightRect);

    clip->Rect = rect;

    TransformGroup ^ group = dynamic_cast<TransformGroup^>(imgCard->RenderTransform);

    if ( group != nullptr )
    {
        for (unsigned int idx = 0; idx < group->Children->Size; idx++)
        {	
            TranslateTransform^ tfrm = dynamic_cast<TranslateTransform^>(group->Children->GetAt(idx));
            if ( tfrm != nullptr )
            {
                tfrm->X = -x * CardWidthRect * SCALE_FACTOR;
                tfrm->Y = -y * CardHeightRect * SCALE_FACTOR;
            }

            ScaleTransform ^scale = dynamic_cast<ScaleTransform^>(group->Children->GetAt(idx));
            if ( scale != nullptr )
            {
                scale->ScaleX = SCALE_FACTOR;
                scale->ScaleY = SCALE_FACTOR;
                scale->CenterX = - CardWidthRect / SCALE_FACTOR;
                scale->CenterY = - CardHeightRect / SCALE_FACTOR;
            }
        }
    }
}

CardShape::~CardShape()
{
}
