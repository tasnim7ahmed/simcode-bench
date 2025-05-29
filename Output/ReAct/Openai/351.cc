#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable logging (optional)
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer wifiNodes;
    wifiNodes.Create(2);

    // Configure Wi-Fi channel and PHY
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211g);

    WifiMacHelper wifiMac;

    // Configure AP
    Ssid ssid = Ssid("ns3-wifi");

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid),
                    "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevice;
    staDevice = wifiHelper.Install(wifiPhy, wifiMac, wifiNodes.Get(1)); // Client

    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice;
    apDevice = wifiHelper.Install(wifiPhy, wifiMac, wifiNodes.Get(0)); // AP

    // Mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiNodes);

    Ptr<MobilityModel> apMobility = wifiNodes.Get(0)->GetObject<MobilityModel>();
    apMobility->SetPosition(Vector(0.0, 0.0, 0.0));
    Ptr<MobilityModel> staMobility = wifiNodes.Get(1)->GetObject<MobilityModel>();
    staMobility->SetPosition(Vector(5.0, 0.0, 0.0));

    // Internet Stack
    InternetStackHelper stack;
    stack.Install(wifiNodes);

    // IP Address assignment
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterface = address.Assign(apDevice);
    Ipv4InterfaceContainer staInterface = address.Assign(staDevice);

    // Applications
    uint16_t port = 8080;

    // UDP Server (on AP)
    UdpServerHelper udpServer(port);
    ApplicationContainer serverApp = udpServer.Install(wifiNodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP Client (on STA)
    UdpClientHelper udpClient(apInterface.GetAddress(0), port);
    udpClient.SetAttribute("MaxPackets", UintegerValue(20));
    udpClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    udpClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = udpClient.Install(wifiNodes.Get(1));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run simulation
    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}