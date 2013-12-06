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
* MainPage.xaml.h - Declaration of the MainPage class.  Also includes
* the declaration for the FacebookAlbum class that the GridView databinds to.
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#include "MainPage.g.h"

namespace FacebookDemo
{
	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	public ref class MainPage sealed
	{
	public:
		MainPage();

	protected:
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

	private:
		void LoginButton_Click_1(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void AlbumButton_Click_1(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
	};

	[Windows::UI::Xaml::Data::Bindable]
	[Windows::Foundation::Metadata::WebHostHidden]
	public ref class FacebookAlbum sealed
	{
	internal:
		FacebookAlbum(std::wstring title, int count, std::wstring id, std::wstring photo_id):
			title_(title), count_(count), id_(id), photo_id_(photo_id) {}

	public:
		property Platform::String^ Title {
			Platform::String^ get();
		}

		property int Count {
			int get();
		}

		property Windows::UI::Xaml::Media::ImageSource^ Preview {
			Windows::UI::Xaml::Media::ImageSource^ get();
		}
		
	private:
		std::wstring id_;
		std::wstring title_;
		std::wstring photo_id_;
		int count_;
		Windows::UI::Xaml::Media::ImageSource^ preview_;
	};
}
