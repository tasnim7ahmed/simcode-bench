#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiMultiApSimulation");

int main(int argc, char *argv[]) {
    uint32_t numAp = 3;
    uint32_t numStaPerAp = 2;
    uint32_t numSta = numAp * numStaPerAp;
    double simTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);

    NodeContainer apNodes;
    apNodes.Create(numAp);

    NodeContainer staNodes;
    staNodes.Create(numSta);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    WifiMacHelper mac;
    Ssid ssid;

    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices;

    for (uint32_t i = 0; i < numAp; ++i) {
        std::ostringstream oss;
        oss << "wifi-network-" << i;
        ssid = Ssid(oss.str());
        mac.SetType("ns3::ApWifiMac",
                    "Ssid", SsidValue(ssid),
                    "BeaconInterval", TimeValue(Seconds(2.5)));
        NetDeviceContainer dev = wifi.Install(phy, mac, apNodes.Get(i));
        apDevices.Add(dev);
    }

    for (uint32_t i = 0; i < numSta; ++i) {
        uint32_t apIndex = i / numStaPerAp;
        std::ostringstream oss;
        oss << "wifi-network-" << apIndex;
        ssid = Ssid(oss.str());
        mac.SetType("ns3::StaWifiMac",
                    "Ssid", SsidValue(ssid));
        NetDeviceContainer dev = wifi.Install(phy, mac, staNodes.Get(i));
        staDevices.Add(dev);
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(-50, 50, -50, 50)));
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");

    Ipv4InterfaceContainer apInterfaces;
    Ipv4InterfaceContainer staInterfaces;

    apInterfaces = address.Assign(apDevices);
    staInterfaces = address.Assign(staDevices);

    NodeContainer remoteServer;
    remoteServer.Create(1);
    stack.Install(remoteServer);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer serverDevices;
    for (uint32_t i = 0; i < apNodes.GetN(); ++i) {
        NetDeviceContainer link = p2p.Install(remoteServer.Get(0), apNodes.Get(i));
        serverDevices.Add(link.Get(0));
    }

    Ipv4InterfaceContainer serverInterfaces;
    for (uint32_t i = 0; i < serverDevices.GetN(); ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer ifc = address.Assign(serverDevices.Get(i));
        serverInterfaces.Add(ifc);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    onoff.SetConstantRate(DataRate("1Mbps"), 1024);
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(simTime)));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < staInterfaces.GetN(); ++i) {
        Address sinkAddress(InetSocketAddress(serverInterfaces.GetAddress(0), port));
        PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sink.Install(remoteServer.Get(0));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simTime));

        onoff.SetAttribute("Remote", AddressValue(sinkAddress));
        ApplicationContainer app = onoff.Install(staNodes.Get(i));
        clientApps.Add(app);
    }

    AsciiTraceHelper ascii;
    phy.EnableAsciiAll(ascii.CreateFileStream("wifi-multi-ap.tr"));
    phy.EnablePcapAll("wifi-multi-ap");

    AnimationInterface anim("wifi-multi-ap.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}