#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Enable logging for debugging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create three nodes
    NodeContainer nodes;
    nodes.Create(3);

    // Configure Wi-Fi with 802.11n
    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));

    wifiMac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(Ssid("wifi-network")),
                    "ActiveProbing", BooleanValue(false));

    NetDeviceContainer devices = wifi.Install(nodes);

    // Set up access point (for simplicity, use Node 1 as AP)
    wifiMac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(Ssid("wifi-network")));
    wifi.Install(nodes.Get(1), nodes.Get(1)->GetDevice(0));

    // Positioning in grid layout using ConstantPositionMobilityModel
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Assign positions manually
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));   // Node 0
    positionAlloc->Add(Vector(10.0, 0.0, 0.0));  // Node 1 (AP)
    positionAlloc->Add(Vector(20.0, 0.0, 0.0));  // Node 2
    MobilityModel* mob;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        mob = nodes.Get(i)->GetObject<MobilityModel>();
        mob->SetPosition(positionAlloc->GetPosition(i));
    }

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP Echo Server on Node 0
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Set up UDP Echo Client on Node 2
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(2));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}