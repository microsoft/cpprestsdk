using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System;
using System.Collections;
using System.Collections.Specialized;
using System.IO;
using System.Net;
using System.Reflection;
using System.Text;
using System.Threading;

class Program
{
    /// <summary>
    /// Checks if the appropriate authentication scheme was used.
    /// </summary>
    /// <param name="listenerAuthScheme"></param>
    /// <param name="requestContext"></param>
    /// <returns>True if verification was successful, false otherwise.</returns>
    public static bool VerifyAuthenticationScheme(AuthenticationSchemes listenerAuthScheme, HttpListenerContext requestContext)
    {
        System.Security.Principal.IIdentity id = null;
        if (requestContext.User != null)
        {
            id = requestContext.User.Identity;
            Console.WriteLine("IsAuthenticated:{0}", id.IsAuthenticated);
            Console.WriteLine("AuthenticationType:{0}", id.AuthenticationType);
            Console.WriteLine("Name:{0}", id.Name);

            if (requestContext.Request.Headers.AllKeys.Contains("UserName"))
            {
                if (string.Compare(requestContext.Request.Headers["UserName"], id.Name, true) != 0)
                {
                    return false;
                }
            }
        }

        switch (listenerAuthScheme)
        {
            case AuthenticationSchemes.Anonymous:
                if (id != null)
                {
                    return false;
                }
                break;
            case AuthenticationSchemes.Basic:
                if (!id.IsAuthenticated || id.AuthenticationType != "Basic")
                {
                    return false;
                }
                HttpListenerBasicIdentity basicId = (HttpListenerBasicIdentity)id;
                if (string.Compare(basicId.Password, requestContext.Request.Headers["Password"], true) != 0)
                {
                    return false;
                }
                break;
            case AuthenticationSchemes.Digest:
                if (!id.IsAuthenticated || id.AuthenticationType != "Digest")
                {
                    return false;
                }
                break;
            case AuthenticationSchemes.Ntlm:
                if (!id.IsAuthenticated || id.AuthenticationType != "NTLM")
                {
                    return false;
                }
                break;
            case AuthenticationSchemes.Negotiate:
                if (!id.IsAuthenticated || id.AuthenticationType != "NTLM")
                {
                    return false;
                }
                break;
            case AuthenticationSchemes.IntegratedWindowsAuthentication:
                if (!id.IsAuthenticated || id.AuthenticationType != "NTLM")
                {
                    return false;
                }
                break;
            default:
                return false;
        }
        return true;
    }

    static void Main(string []args)
    {
        if(args.Length != 2)
        {
            Console.WriteLine("Error: please specify URI to listen on and Authentication type to use:\nAuthListener.exe ServerURI AuthenticationScheme");
            return;
        }
        string serverUri = args[0];
        AuthenticationSchemes authScheme;
        switch(args[1].ToLower())
        {
            case "none":
                authScheme = AuthenticationSchemes.None;
                break;
            case "anonymous":
                authScheme = AuthenticationSchemes.Anonymous;
                break;
            case "basic":
                authScheme = AuthenticationSchemes.Basic;
                break;
            case "digest":
                authScheme = AuthenticationSchemes.Digest;
                break;
            case "ntlm":
                authScheme = AuthenticationSchemes.Ntlm;
                break;
            case "negotiate":
                authScheme = AuthenticationSchemes.Negotiate;
                break;
            case "integrated":
                authScheme = AuthenticationSchemes.IntegratedWindowsAuthentication;
                break;
            default:
                Console.WriteLine("Error: unrecognized AuthenticationScheme:{0}", args[1]);
                return;
        }
    
        HttpListener listener = new HttpListener();
        listener.Prefixes.Add(serverUri);
        listener.AuthenticationSchemes = authScheme;

        listener.Start();
        Console.WriteLine("Listening...");

		HttpListenerContext context = listener.GetContext();
		Console.WriteLine("Received request...");
		StreamWriter writer = new StreamWriter(context.Response.OutputStream);

		// Verify correct authentication scheme was used.
		if (!VerifyAuthenticationScheme(listener.AuthenticationSchemes, context))
		{
			context.Response.StatusCode = (int)HttpStatusCode.ExpectationFailed;
			writer.Write("Error authentication validation failed");
		}

		context.Response.Close();
	}
}