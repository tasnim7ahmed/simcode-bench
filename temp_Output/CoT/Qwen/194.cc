#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiUdpSimulation");

int main(int argc, char *argv[]) {
    uint32_t numStationsPerAp = 5;
    double simulationTime = 10.0;
    std::string phyMode("DsssRate1Mbps");

    CommandLine cmd(__FILE__);
    cmd.AddValue("numStationsPerAp", "Number of stations per access point", numStationsPerAp);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

    NodeContainer apNodes;
    apNodes.Create(2);

    NodeContainer staNodes1;
    staNodes1.Create(numStationsPerAp);

    NodeContainer staNodes2;
    staNodes2.Create(numStationsPerAp);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));

    WifiMacHelper mac;
    Ssid ssid1 = Ssid("wifi-network-1");
    Ssid ssid2 = Ssid("wifi-network-2");

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid1),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices1 = wifi.Install(phy, mac, apNodes.Get(0));

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid2),
                "BeaconInterval", TimeValue(Seconds(2.5)));
    NetDeviceContainer apDevices2 = wifi.Install(phy, mac, apNodes.Get(1));

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid1),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices1 = wifi.Install(phy, mac, staNodes1);

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid2),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices2 = wifi.Install(phy, mac, staNodes2);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes1);
    mobility.Install(staNodes2);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces1 = address.Assign(apDevices1);
    Ipv4InterfaceContainer staInterfaces1 = address.Assign(staDevices1);

    address.NewNetwork();
    Ipv4InterfaceContainer apInterfaces2 = address.Assign(apDevices2);
    Ipv4InterfaceContainer staInterfaces2 = address.Assign(staDevices2);

    address.NewNetwork();
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NodeContainer csmaNodes;
    csmaNodes.Add(apNodes);
    csmaNodes.Create(1); // Server node

    PointToPointHelper p2pServer;
    p2pServer.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2pServer.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer serverDevice = p2pServer.Install(csmaNodes.Get(2), csmaNodes.Get(0));
    Ipv4InterfaceContainer serverInterface = address.Assign(serverDevice.Get(0));

    NetDeviceContainer serverLink2 = p2p.Install(csmaNodes.Get(0), csmaNodes.Get(1));
    Ipv4InterfaceContainer serverInterface2 = address.Assign(serverLink2.Get(0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApps = echoServer.Install(csmaNodes.Get(2));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    for (uint32_t i = 0; i < staInterfaces1.GetN(); ++i) {
        UdpEchoClientHelper echoClient(serverInterface.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(staNodes1.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simulationTime));
    }

    for (uint32_t i = 0; i < staInterfaces2.GetN(); ++i) {
        UdpEchoClientHelper echoClient(serverInterface.GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.1)));
        echoClient.SetAttribute("PacketSize", UintegerValue(1024));

        ApplicationContainer clientApps = echoClient.Install(staNodes2.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simulationTime));
    }

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi_udp_simulation.tr"));
    phy.EnablePcapAll("wifi_udp_simulation");

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    for (uint32_t i = 0; i < staDevices1.GetN(); ++i) {
        Ptr<NetDevice> device = staDevices1.Get(i);
        Ptr<QueueDiscItem> item;
        uint64_t txBytes = 0;
        while (device->GetObject<PointToPointNetDevice>()->GetQueue()->Dequeue(item)) {
            txBytes += item->GetSize();
        }
        NS_LOG_UNCOND("STA1 Node " << i << " total data sent: " << txBytes << " bytes");
    }

    for (uint32_t i = 0; i < staDevices2.GetN(); ++i) {
        Ptr<NetDevice> device = staDevices2.Get(i);
        Ptr<QueueDiscItem> item;
        uint64_t txBytes = 0;
        while (device->GetObject<PointToPointNetDevice>()->GetQueue()->Dequeue(item)) {
            txBytes += item->GetSize();
        }
        NS_LOG_UNCOND("STA2 Node " << i << " total data sent: " << txBytes << " bytes");
    }

    Simulator::Destroy();
    return 0;
}