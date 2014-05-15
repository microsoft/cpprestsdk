//
// MainPage.xaml.h
// Declaration of the MainPage class.
//

#pragma once

#include "MainPage.g.h"

using namespace web::http;
using namespace web::http::client;
using namespace web::http::client::experimental;

namespace OAuth2Live
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
        void StartButtonClick(Platform::Object^ sender, Windows::UI::Xaml::Navigation::NavigationEventArgs^ e);
        void InfoButtonClick(Platform::Object^ sender, Windows::UI::Xaml::Navigation::NavigationEventArgs^ e);
        void ContactsButtonClick(Platform::Object^ sender, Windows::UI::Xaml::Navigation::NavigationEventArgs^ e);
        void EventsButtonClick(Platform::Object^ sender, Windows::UI::Xaml::Navigation::NavigationEventArgs^ e);

        void AuthCodeTextChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::TextChangedEventArgs^ e);
        void AccessTokenTextChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::TextChangedEventArgs^ e);

        oauth2_config m_live_oauth2_config;
        std::unique_ptr<http_client> m_live_client;
    };
}
