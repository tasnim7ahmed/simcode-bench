#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MixedWiredWirelessTopology");

int main(int argc, char *argv[]) {
    uint32_t numBackboneRouters = 3;
    uint32_t numLanNodesPerRouter = 2;
    uint32_t numWirelessStasPerAp = 2;
    double simulationTime = 10.0;
    bool enableMobility = true;
    bool enableTracing = true;
    bool enableAnimation = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numBackboneRouters", "Number of backbone routers in ad hoc network", numBackboneRouters);
    cmd.AddValue("numLanNodesPerRouter", "Number of LAN nodes per backbone router", numLanNodesPerRouter);
    cmd.AddValue("numWirelessStasPerAp", "Number of wireless stations per infrastructure network", numWirelessStasPerAp);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("enableMobility", "Enable mobility for all nodes", enableMobility);
    cmd.AddValue("enableTracing", "Enable pcap tracing", enableTracing);
    cmd.AddValue("enableAnimation", "Enable NetAnim XML trace file", enableAnimation);
    cmd.Parse(argc, argv);

    NodeContainer backboneNodes;
    backboneNodes.Create(numBackboneRouters);

    std::vector<NodeContainer> lanNodes(numBackboneRouters);
    std::vector<Ptr<Node>> lanSwitches(numBackboneRouters);
    std::vector<NodeContainer> apNodes(numBackboneRouters);
    std::vector<NodeContainer> staNodes(numBackboneRouters);

    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        lanSwitches[i] = CreateObject<Node>();
        lanNodes[i].Create(numLanNodesPerRouter);
        apNodes[i].Create(1); // AP
        staNodes[i].Create(numWirelessStasPerAp);
    }

    PointToPointHelper p2pLan;
    p2pLan.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pLan.SetChannelAttribute("Delay", StringValue("2ms"));

    std::vector<NetDeviceContainer> backboneDevices(numBackboneRouters);
    std::vector<NetDeviceContainer> lanSwitchDevs(numBackboneRouters);
    std::vector<std::vector<NetDeviceContainer>> lanHostDevs(numBackboneRouters);
    std::vector<WifiNetDevice> apDev(numBackboneRouters);
    std::vector<NetDeviceContainer> staDevs(numBackboneRouters);

    InternetStackHelper stack;
    Ipv4AddressHelper address;
    OlsrHelper olsr;

    stack.SetRoutingHelper(olsr);
    stack.Install(backboneNodes);
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        stack.Install(lanSwitches[i]);
        stack.Install(lanNodes[i]);
        stack.Install(apNodes[i]);
        stack.Install(staNodes[i]);
    }

    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    // Backbone Ad Hoc Network
    wifiMac.SetType("ns3::AdhocWifiMac");
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        backboneDevices[i] = wifi.Install(wifiPhy, wifiMac, backboneNodes.Get(i));
    }

    // Infrastructure networks
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(Ssid("infrastructure-" + std::to_string(i))));
        staDevs[i] = wifi.Install(wifiPhy, wifiMac, staNodes[i]);

        wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(Ssid("infrastructure-" + std::to_string(i))));
        NetDeviceContainer apDevCont = wifi.Install(wifiPhy, wifiMac, apNodes[i].Get(0));
        apDev[i] = DynamicCast<WifiNetDevice>(apDevCont.Get(0));
    }

    // Connect LANs
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        lanHostDevs[i].resize(numLanNodesPerRouter);
        for (uint32_t j = 0; j < numLanNodesPerRouter; ++j) {
            NetDeviceContainer devs = p2pLan.Install(lanSwitches[i], lanNodes[i].Get(j));
            lanHostDevs[i][j] = devs;
        }
        lanSwitchDevs[i] = p2pLan.Install(backboneNodes.Get(i), lanSwitches[i]);
    }

    // Assign IP addresses
    address.SetBase("10.1.0.0", "255.255.255.0");
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        address.Assign(backboneDevices[i]);
        address.NewNetwork();
    }

    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        address.Assign(apDev[i]);
        address.NewNetwork();
        address.Assign(staDevs[i]);
        address.NewNetwork();
        address.Assign(lanSwitchDevs[i]);
        address.NewNetwork();
        for (uint32_t j = 0; j < numLanNodesPerRouter; ++j) {
            address.Assign(lanHostDevs[i][j]);
            address.NewNetwork();
        }
    }

    // Mobility
    if (enableMobility) {
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
        mobility.Install(backboneNodes);
        for (uint32_t i = 0; i < numBackboneRouters; ++i) {
            mobility.Install(apNodes[i]);
            mobility.Install(staNodes[i]);
        }
    }

    // UDP Flow
    uint16_t port = 9;
    Address sinkAddress;
    InetSocketAddress sinkSocketAddr;

    if (numBackboneRouters > 0) {
        uint32_t lastIdx = numBackboneRouters - 1;
        Ipv4Address remoteIp = staNodes[lastIdx].Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
        sinkSocketAddr = InetSocketAddress(remoteIp, port);
        sinkAddress = InetSocketAddress(remoteIp, port);
    }

    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps;
    for (uint32_t i = 0; i < numBackboneRouters; ++i) {
        for (uint32_t j = 0; j < numWirelessStasPerAp; ++j) {
            sinkApps.Add(packetSinkHelper.Install(staNodes[i].Get(j)));
        }
    }
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simulationTime));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer sourceApps;
    sourceApps.Add(onoff.Install(lanNodes[0].Get(0)));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(simulationTime));

    // Tracing
    if (enableTracing) {
        wifiPhy.EnablePcapAll("mixed_topology");
    }

    if (enableAnimation) {
        AnimationInterface anim("mixed_topology.xml");
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}