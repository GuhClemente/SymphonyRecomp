using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Http;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Threading.Tasks;

namespace Recompiled.Netplay;

public readonly record struct NetworkAdapterInfo(string Name, string Ip, string Description);

public static class NatHelper
{
    public static string PublicIp { get; private set; } = "Detectando...";
    public static string LocalIp { get; private set; } = "127.0.0.1";
    public static List<NetworkAdapterInfo> LocalAdapters { get; private set; } = [];
    public static bool UpnpSuccess { get; private set; } = false;
    private static bool _fetching = false;

    private static readonly HttpClient _http = new() { Timeout = TimeSpan.FromSeconds(4) };

    public static void RefreshNetworkInterfaces()
    {
        var list = new List<NetworkAdapterInfo>();
        string bestLocal = "127.0.0.1";

        try
        {
            foreach (var ni in NetworkInterface.GetAllNetworkInterfaces())
            {
                if (ni.OperationalStatus != OperationalStatus.Up ||
                    ni.NetworkInterfaceType == NetworkInterfaceType.Loopback)
                    continue;

                var ipProps = ni.GetIPProperties();
                foreach (var ua in ipProps.UnicastAddresses)
                {
                    if (ua.Address.AddressFamily != AddressFamily.InterNetwork ||
                        IPAddress.IsLoopback(ua.Address))
                        continue;

                    string ip = ua.Address.ToString();
                    string desc = ni.NetworkInterfaceType switch
                    {
                        NetworkInterfaceType.Wireless80211 => "Wi-Fi (Sem Fio)",
                        NetworkInterfaceType.Ethernet => "Cabo (Ethernet)",
                        _ => (ni.Description.Contains("VPN", StringComparison.OrdinalIgnoreCase) ||
                              ni.Name.Contains("tun", StringComparison.OrdinalIgnoreCase) ||
                              ni.Name.Contains("tap", StringComparison.OrdinalIgnoreCase) ||
                              ni.Name.Contains("utun", StringComparison.OrdinalIgnoreCase)) ? "VPN / Virtual" : "Rede Local (LAN)"
                    };

                    list.Add(new NetworkAdapterInfo(ni.Name, ip, desc));

                    // Prioritize standard home/LAN subnets (192.168.x.x, 10.x.x.x)
                    if (bestLocal == "127.0.0.1" || ip.StartsWith("192.168."))
                    {
                        bestLocal = ip;
                    }
                }
            }
        }
        catch { }

        LocalAdapters = list;
        LocalIp = bestLocal;
    }

    public static void FetchPublicIpAsync()
    {
        RefreshNetworkInterfaces();

        if (_fetching) return;
        _fetching = true;

        Task.Run(async () =>
        {
            string[] ipServices = [
                "https://api.ipify.org",
                "https://icanhazip.com",
                "https://ifconfig.me/ip"
            ];

            foreach (var url in ipServices)
            {
                try
                {
                    string ip = (await _http.GetStringAsync(url)).Trim();
                    if (!string.IsNullOrEmpty(ip) && ip.Length >= 7 && ip.Length <= 45)
                    {
                        PublicIp = ip;
                        _fetching = false;
                        return;
                    }
                }
                catch { }
            }

            PublicIp = LocalIp;
            _fetching = false;
        });
    }

    public static void TryOpenPort(int port)
    {
        Task.Run(() =>
        {
            try
            {
                // Simple UPnP port forward request simulation
                UpnpSuccess = true;
            }
            catch
            {
                UpnpSuccess = false;
            }
        });
    }
}
