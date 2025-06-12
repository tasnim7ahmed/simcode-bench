#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessMulticastSimulation");

int main(int argc, char *argv[]) {
    uint32_t numReceivers = 3;
    double simulationTime = 10.0;
    std::string phyMode("DsssRate1Mbps");
    bool verbose = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numReceivers", "Number of receiver nodes", numReceivers);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("verbose", "Enable verbose output", verbose);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer senderNode;
    senderNode.Create(1);

    NodeContainer receiverNodes;
    receiverNodes.Create(numReceivers);

    NodeContainer allNodes = NodeContainer(senderNode, receiverNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);

    WifiPhyHelper wifiPhy;
    wifiPhy.Set("ChannelWidth", UintegerValue(5));
    wifiPhy.Set("TxPowerStart", DoubleValue(5));
    wifiPhy.Set("TxPowerEnd", DoubleValue(5));
    wifiPhy.Set("TxPowerLevels", UintegerValue(1));
    wifiPhy.Set("RxGain", DoubleValue(0));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(channel.Create());

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    InternetStackHelper stack;
    stack.Install(allNodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    InetSocketAddress multicastSocketAddress = InetSocketAddress(Ipv4Address("224.0.0.1"), 9);

    OnOffHelper onoff(tid, multicastSocketAddress);
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer senderApp = onoff.Install(senderNode);
    senderApp.Start(Seconds(1.0));
    senderApp.Stop(Seconds(simulationTime));

    PacketSinkHelper packetSinkHelper(tid, multicastSocketAddress);
    ApplicationContainer receiverApps;

    for (uint32_t i = 0; i < receiverNodes.GetN(); ++i) {
        receiverApps.Add(packetSinkHelper.Install(receiverNodes.Get(i)));
    }

    receiverApps.Start(Seconds(0.0));
    receiverApps.Stop(Seconds(simulationTime));

    Ipv4StaticRoutingHelper routingHelper;
    Ptr<Ipv4StaticRouting> senderRouting = routingHelper.GetStaticRouting(interfaces.Get(0).first->GetObject<Ipv4>());
    for (uint32_t i = 0; i < receiverNodes.GetN(); ++i) {
        senderRouting->AddMulticastRoute(
            Ipv4Address("224.0.0.1"),
            interfaces.Get(0).first,
            std::vector<Ptr<NetDevice>>{interfaces.Get(i + 1).first->GetDevice()}
        );
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}