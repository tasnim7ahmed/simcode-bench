#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Wifi80211nTosExample");

int main (int argc, char *argv[])
{
    // Simulation parameters (defaults)
    uint32_t nStations = 4;
    uint32_t mcs = 7; // 0-7 valid for HT
    uint32_t channelWidth = 20; // MHz: 20 or 40
    bool shortGuardInterval = false;
    double distance = 5.0; // meters
    bool useRtsCts = false;
    double simulationTime = 10.0; // seconds

    CommandLine cmd;
    cmd.AddValue ("nStations", "Number of Wi-Fi stations", nStations);
    cmd.AddValue ("mcs", "HT MCS index (0-7)", mcs);
    cmd.AddValue ("channelWidth", "Channel width (MHz, 20 or 40)", channelWidth);
    cmd.AddValue ("shortGuardInterval", "Enable short guard interval (true/false)", shortGuardInterval);
    cmd.AddValue ("distance", "Distance between AP and each station", distance);
    cmd.AddValue ("useRtsCts", "Enable RTS/CTS for data packets", useRtsCts);
    cmd.AddValue ("simulationTime", "Simulation time (seconds)", simulationTime);
    cmd.Parse (argc, argv);

    if (mcs > 7)
    {
      NS_ABORT_MSG ("Valid MCS for 802.11n single stream: 0-7");
    }

    // Create nodes
    NodeContainer wifiStaNodes, wifiApNode;
    wifiStaNodes.Create (nStations);
    wifiApNode.Create (1);

    // Set up Wi-Fi PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
    phy.SetChannel (channel.Create ());
    phy.Set ("ChannelWidth", UintegerValue (channelWidth));
    phy.Set ("ShortGuardEnabled", BooleanValue (shortGuardInterval));

    // Set up Wi-Fi
    WifiHelper wifi;
    wifi.SetStandard (WIFI_STANDARD_80211n_2_4GHZ);

    // HT Configuration
    WifiMacHelper mac;
    HtWifiMacHelper htMac = HtWifiMacHelper::Default (); // For 802.11n HT
    WifiRemoteStationManagerHelper stationManager;
    stationManager.Set minSupportedRate (WifiMode ("HtMcs" + std::to_string (mcs)));
    stationManager.Set maxSupportedRate (WifiMode ("HtMcs" + std::to_string (mcs)));
    wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue ("HtMcs" + std::to_string (mcs)),
                                  "ControlMode", StringValue ("HtMcs0"));

    if (useRtsCts)
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (0));
    }
    else
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue (2347));
    }

    Ssid ssid = Ssid ("wifi-tos-ssid");

    // Set up MAC for STA devices, with multiple TOS values (differentiated ACs)
    NetDeviceContainer staDevices;
    NetDeviceContainer apDevice;
    for (uint32_t i = 0; i < nStations; ++i)
    {
      htMac.SetType ("ns3::StaWifiMac",
                    "Ssid", SsidValue (ssid),
                    "ActiveProbing", BooleanValue (false));
      staDevices.Add (wifi.Install (phy, htMac, wifiStaNodes.Get (i)));
    }
    htMac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
    apDevice = wifi.Install (phy, htMac, wifiApNode);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
    positionAlloc->Add (Vector (0.0, 0.0, 0.0));
    for (uint32_t i = 0; i < nStations; ++i)
    {
      double angle = (2*M_PI*i)/nStations;
      positionAlloc->Add (Vector (distance * std::cos (angle), distance * std::sin (angle), 0.0));
    }
    mobility.SetPositionAllocator (positionAlloc);
    mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
    mobility.Install (wifiApNode);
    mobility.Install (wifiStaNodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install (wifiApNode);
    stack.Install (wifiStaNodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign (apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign (staDevices);

    // UDP traffic setup: for each station, one traffic flow with a different TOS value (DSCP)
    uint16_t port = 5000;
    ApplicationContainer serverApps, clientApps;
    for (uint32_t i = 0; i < nStations; ++i)
    {
      // UdpServer on AP
      UdpServerHelper server (port + i);
      serverApps.Add (server.Install (wifiApNode.Get (0)));

      // UdpClient on STA->AP, each with unique DSCP value per station (AFxx, e.g.)
      UdpClientHelper client (apInterface.GetAddress (0), port + i);
      client.SetAttribute ("MaxPackets", UintegerValue (uint32_t(-1)));
      client.SetAttribute ("Interval", TimeValue (Seconds (0.0005)));
      client.SetAttribute ("PacketSize", UintegerValue (1200));

      // Set TOS field in IP header (use DSCP AFxx: AF11=0x28, AF12=0x30, AF13=0x38, AF21=0x48 ...)
      uint8_t tos = (i % 4 == 0) ? 0x28 : (i % 4 == 1) ? 0x30 : (i % 4 == 2) ? 0x38 : 0x48; // AF11, AF12, AF13, AF21
      client.SetAttribute ("TypeOfService", UintegerValue (tos));

      clientApps.Add (client.Install (wifiStaNodes.Get (i)));
    }
    serverApps.Start (Seconds (0.0));
    serverApps.Stop (Seconds (simulationTime));
    clientApps.Start (Seconds (1.0));
    clientApps.Stop (Seconds (simulationTime));

    // Throughput calculation
    Simulator::Stop (Seconds (simulationTime));

    // Enable logging (optional)
    // LogComponentEnable ("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable ("UdpServer", LOG_LEVEL_INFO);

    // Run simulation
    Simulator::Run ();

    // Collect statistics
    double totalReceived = 0;
    for (uint32_t i = 0; i < nStations; ++i)
    {
      Ptr<UdpServer> server = DynamicCast<UdpServer>(serverApps.Get(i));
      uint64_t bytes = server->GetReceived () * 1200; // number of packets * packet size (bytes)
      totalReceived += bytes;
    }
    double throughputMbps = (totalReceived * 8.0) / (simulationTime - 1.0) / 1e6; // exclude 1s start delay

    std::cout << "Aggregated UDP throughput (Mbit/s): " << throughputMbps << std::endl;
    std::cout << "Parameters: nStations=" << nStations
              << " mcs=" << mcs
              << " channelWidth=" << channelWidth
              << " SGI=" << (shortGuardInterval ? "short" : "long")
              << " rtsCts=" << (useRtsCts ? "enabled" : "disabled")
              << " distance=" << distance << "m"
              << std::endl;

    Simulator::Destroy();
    return 0;
}