//
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"

using namespace OAuth2Live;

using namespace Concurrency;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::Security::Authentication::Web;


static const utility::string_t s_live_key = L"";
static const utility::string_t s_live_secret = L"";

static const utility::string_t s_state = L"1234ABCD";


MainPage::MainPage()
    : m_live_oauth2_config(
        s_live_key,
        s_live_secret,
        L"https://login.live.com/oauth20_authorize.srf",
        L"https://login.live.com/oauth20_token.srf",
        L"https://login.live.com/oauth20_desktop.srf")
{
    InitializeComponent();
}

/// <summary>
/// Invoked when this page is about to be displayed in a Frame.
/// </summary>
/// <param name="e">Event data that describes how this page was reached.  The Parameter
/// property is typically used to configure the page.</param>
void MainPage::OnNavigatedTo(NavigationEventArgs^ e)
{
	(void) e;	// Unused parameter
}

void OAuth2Live::MainPage::StartButtonClick(Platform::Object^ sender, Windows::UI::Xaml::Navigation::NavigationEventArgs^ e)
{
    m_live_oauth2_config.set_scope(L"wl.basic wl.calendars");
    m_live_oauth2_config.set_state(s_state);

    String^ authURI = ref new String(m_live_oauth2_config.build_authorization_uri().c_str());
    auto startURI = ref new Uri(authURI);
    String^ redirectURI = ref new String(m_live_oauth2_config.redirect_uri().c_str());
    auto endURI = ref new Uri(redirectURI);

    try
    {
        DebugArea->Text += "> Navigating WebAuthenticationBroker to " + authURI + "...\n";

#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
		WebAuthenticationBroker::AuthenticateAndContinue(startURI, endURI, nullptr, WebAuthenticationOptions::None);
#else
        concurrency::create_task(WebAuthenticationBroker::AuthenticateAsync(WebAuthenticationOptions::None, startURI, endURI))
            .then([this](WebAuthenticationResult^ result)
        {
            String^ statusString;

            DebugArea->Text += "< WebAuthenticationBroker returned: ";
            switch (result->ResponseStatus)
            {
                case WebAuthenticationStatus::ErrorHttp:
                {
                    DebugArea->Text += "ErrorHttp: " + result->ResponseErrorDetail + "\n";
                    break;
                }
                case WebAuthenticationStatus::Success:
                {
                    DebugArea->Text += "Success\n";
                    utility::string_t data = result->ResponseData->Data();
                    try
                    {
                        DebugArea->Text += "Redirect URI:\n" + result->ResponseData + "\n";
                        DebugArea->Text += "> Obtaining token from redirect...\n";
                        m_live_oauth2_config.token_from_redirected_uri(data).then([this]()
                        {
                            DebugArea->Text += "< Got token.\n";
                            AccessToken->Text = ref new String(m_live_oauth2_config.token().c_str());
                        }, pplx::task_continuation_context::use_current());
                    }
                    catch (oauth2_exception& e)
                    {
                        String^ error = ref new String(utility::conversions::to_string_t(e.what()).c_str());
                        DebugArea->Text += "Error: " + error + "\n";
                    }
                    break;
                }
                default:
                case WebAuthenticationStatus::UserCancel:
                {
                    DebugArea->Text += "UserCancel\n";
                    break;
                }
            }
        });
#endif
    }
    catch (Exception^ ex)
    {
        DebugArea->Text += "< Error launching WebAuthenticationBroker: " + ex->Message + "\n";
    }
}

void OAuth2Live::MainPage::InfoButtonClick(Platform::Object^ sender, Windows::UI::Xaml::Navigation::NavigationEventArgs^ e)
{
    DebugArea->Text += "> Get user info...\n";
    m_live_client->request(methods::GET, U("me"))
    .then([](http_response resp)
    {
        return resp.extract_json();
    })
    .then([this](web::json::value j) -> void
    {
        String^ json_code = ref new String(j.serialize().c_str());
        DebugArea->Text += "< User info (JSON): " + json_code + "\n";
    }, pplx::task_continuation_context::use_current());
}

void OAuth2Live::MainPage::ContactsButtonClick(Platform::Object^ sender, Windows::UI::Xaml::Navigation::NavigationEventArgs^ e)
{
    DebugArea->Text += "> Get user contacts...\n";
    m_live_client->request(methods::GET, U("me/contacts"))
    .then([](http_response resp)
    {
        return resp.extract_json();
    })
    .then([this](web::json::value j) -> void
    {
        String^ json_code = ref new String(j.serialize().c_str());
        DebugArea->Text += "< User contacts (JSON): " + json_code + "\n";
    }, pplx::task_continuation_context::use_current());
}

void OAuth2Live::MainPage::EventsButtonClick(Platform::Object^ sender, Windows::UI::Xaml::Navigation::NavigationEventArgs^ e)
{
    DebugArea->Text += "> Get user events...\n";
    m_live_client->request(methods::GET, U("me/events"))
    .then([](http_response resp)
    {
        return resp.extract_json();
    })
    .then([this](web::json::value j) -> void
    {
        String^ json_code = ref new String(j.serialize().c_str());
        DebugArea->Text += "< User calendar events (JSON): " + json_code + "\n";
    }, pplx::task_continuation_context::use_current());
}

void OAuth2Live::MainPage::AuthCodeTextChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::TextChangedEventArgs^ e)
{
    DebugArea->Text += "> Fetching token...\n";
    utility::string_t code(AuthorizationCode->Text->Data());
    m_live_oauth2_config.fetch_token(code).then([this]() -> void
    {
        String^ token = ref new String(m_live_oauth2_config.token().c_str());
        DebugArea->Text += "< Got token.\n";
        AccessToken->Text = token;
    }, pplx::task_continuation_context::use_current());
}

void OAuth2Live::MainPage::AccessTokenTextChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::TextChangedEventArgs^ e)
{
    http_client_config http_config;
    http_config.set_oauth2(m_live_oauth2_config);
    m_live_client = utility::details::make_unique<http_client>(U("https://apis.live.net/v5.0/"), http_config);

    // Enable buttons since we have token now.
    InfoButton->IsEnabled = true;
    ContactsButton->IsEnabled = true;
    EventsButton->IsEnabled = true;
}

void OAuth2Live::MainPage::ImplicitGrantUnchecked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    m_live_oauth2_config.set_implicit_grant(false);
}

void OAuth2Live::MainPage::ImplicitGrantChecked(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
    m_live_oauth2_config.set_implicit_grant(true);
}
