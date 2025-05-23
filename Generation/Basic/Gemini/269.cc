#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging for applications to see packet flow
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // 1. Create nodes for AP and STAs
    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(3);

    // 2. Configure Mobility Model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    mobility.Install(apNode); // AP at (0,0,0)
    mobility.Install(staNodes); // STAs at (0,5,0), (5,0,0), (5,5,0)

    // 3. Configure Wi-Fi PHY and Channel for 802.11n
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211n_2_4GHZ); // 802.11n standard at 2.4 GHz

    YansWifiPhyHelper phy;
    // Set up a Wifi channel
    phy.SetChannel(YansWifiChannelHelper::Create());

    // 4. Configure MAC for AP (Access Point)
    NqosWifiMacHelper apMac;
    apMac.SetType("ns3::ApWifiMac",
                  "Ssid", SsidValue(Ssid("ns3-wifi")),
                  "BeaconInterval", TimeValue(MicroSeconds(102400))); // Default beacon interval

    NetDeviceContainer apDevice = wifi.Install(phy, apMac, apNode.Get(0));

    // 5. Configure MAC for STAs (Stations)
    NqosWifiMacHelper staMac;
    staMac.SetType("ns3::StaWifiMac",
                   "Ssid", SsidValue(Ssid("ns3-wifi")),
                   "ActiveProbing", BooleanValue(false)); // No active probing needed if SSID known

    NetDeviceContainer staDevices = wifi.Install(phy, staMac, staNodes);

    // 6. Install Internet Stack on all nodes
    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNodes);

    // 7. Assign IPv4 Addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterfaces = address.Assign(staDevices);

    // Get the AP's IP address (assigned to the first device in apDevice container)
    Ipv4Address apIpAddress = apInterface.GetAddress(0);

    // 8. Configure Applications
    // UDP Echo Server on the AP
    UdpEchoServerHelper echoServer(9); // Listen on port 9
    ApplicationContainer serverApps = echoServer.Install(apNode.Get(0));
    serverApps.Start(Seconds(1.0)); // Server starts at 1 second
    serverApps.Stop(Seconds(10.0)); // Server stops at 10 seconds

    // UDP Echo Client on the first STA (staNodes.Get(0))
    UdpEchoClientHelper echoClient(apIpAddress, 9); // Send to AP's IP, port 9
    echoClient.SetAttribute("MaxPackets", UintegerValue(5)); // Send 5 packets
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Send one packet per second
    echoClient.SetAttribute("PacketSize", UintegerValue(512)); // Packet size 512 bytes

    ApplicationContainer clientApps = echoClient.Install(staNodes.Get(0));
    clientApps.Start(Seconds(2.0)); // Client starts at 2 seconds (after server)
    clientApps.Stop(Seconds(9.0)); // Client stops at 9 seconds

    // Set simulation duration
    Simulator::Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy(); // Clean up resources

    return 0;
}