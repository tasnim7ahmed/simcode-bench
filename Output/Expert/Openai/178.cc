#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main(int argc, char *argv[])
{
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Configure wifi
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_2_4GHZ);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    // Use HT configuration (802.11n)
    HtWifiMacHelper htMac;
    HtConfiguration htConf;
    htConf.SetChannelWidth(20);

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue("HtMcs7"),
                                "ControlMode", StringValue("HtMcs0"));

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Internet stack
    InternetStackHelper stack;
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP server on node 1
    uint16_t serverPort = 4000;
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(1));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // UDP client on node 0
    uint32_t maxPackets = 50;
    uint32_t intervalMs = 100;
    uint32_t packetSize = 512;
    UdpClientHelper udpClient(interfaces.GetAddress(1), serverPort);
    udpClient.SetAttribute("MaxPackets", UintegerValue(maxPackets));
    udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(intervalMs)));
    udpClient.SetAttribute("PacketSize", UintegerValue(packetSize));
    ApplicationContainer clientApp = udpClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(10.0));

    // NetAnim setup
    AnimationInterface anim("wifi-adhoc-80211n.xml");
    anim.SetMobilityPollInterval(Seconds(0.25));
    anim.EnablePacketMetadata(true);

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}