#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WiFiMultiAPSimulation");

int main(int argc, char *argv[]) {
    uint32_t numAp = 3;
    uint32_t numStaPerAp = 2;
    uint32_t numSta = numAp * numStaPerAp;
    double simDuration = 10.0;

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    NodeContainer apNodes;
    apNodes.Create(numAp);

    NodeContainer staNodes;
    staNodes.Create(numSta);

    NodeContainer remoteServer;
    remoteServer.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    WifiMacHelper mac;
    Ssid ssid;

    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices[numAp];
    mac.SetType("ns3::ApWifiMac");
    for (uint32_t i = 0; i < numAp; ++i) {
        std::ostringstream oss;
        oss << "ssid-ap-" << i;
        ssid = Ssid(oss.str());
        mac.Set("Ssid", SsidValue(ssid));
        apDevices.Add(wifi.Install(phy, mac, apNodes.Get(i)));
    }

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));
    for (uint32_t i = 0; i < numAp; ++i) {
        std::ostringstream oss;
        oss << "ssid-ap-" << i;
        ssid = Ssid(oss.str());
        mac.Set("Ssid", SsidValue(ssid));
        for (uint32_t j = 0; j < numStaPerAp; ++j) {
            staDevices[i].Add(wifi.Install(phy, mac, staNodes.Get(i * numStaPerAp + j)));
        }
    }

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer serverDevices;
    NetDeviceContainer apToServerDevices[numAp];
    for (uint32_t i = 0; i < numAp; ++i) {
        apToServerDevices[i] = p2p.Install(apNodes.Get(i), remoteServer.Get(0));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer apInterfaces[numAp];
    Ipv4InterfaceContainer staInterfaces[numAp];
    Ipv4InterfaceContainer serverInterfaces[numAp];

    for (uint32_t i = 0; i < numAp; ++i) {
        std::ostringstream network;
        network << "10.1." << i << ".0";
        address.SetBase(network.str().c_str(), "255.255.255.0");

        apInterfaces[i] = address.Assign(apDevices.Get(i));
        staInterfaces[i] = address.Assign(staDevices[i]);

        address.NewNetwork();
        serverInterfaces[i] = address.Assign(apToServerDevices[i]);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    UdpServerHelper udpServer(4000);
    ApplicationContainer serverApp = udpServer.Install(remoteServer.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simDuration));

    UdpClientHelper clientHelper;
    clientHelper.SetAttribute("Port", UintegerValue(4000));
    clientHelper.SetAttribute("MaxPackets", UintegerValue(1000000));
    clientHelper.SetAttribute("PacketSize", UintegerValue(1024));
    clientHelper.SetAttribute("Interval", TimeValue(MilliSeconds(100)));

    for (uint32_t i = 0; i < numSta; ++i) {
        Ptr<Node> sta = staNodes.Get(i);
        Ipv4Address remoteIp = serverInterfaces[0].GetAddress(1); // Remote server IP
        clientHelper.SetAttribute("RemoteAddress", AddressValue(Address(remoteIp)));
        ApplicationContainer clientApp = clientHelper.Install(sta);
        clientApp.Start(Seconds(1.0));
        clientApp.Stop(Seconds(simDuration));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(20.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(staNodes);

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(remoteServer);

    AnimationInterface anim("wifi-multiap-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));
    for (uint32_t i = 0; i < numAp; ++i) {
        anim.UpdateNodeDescription(apNodes.Get(i), "AP" + std::to_string(i));
    }
    for (uint32_t i = 0; i < numSta; ++i) {
        anim.UpdateNodeDescription(staNodes.Get(i), "STA" + std::to_string(i));
    }
    anim.UpdateNodeDescription(remoteServer.Get(0), "Server");

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}