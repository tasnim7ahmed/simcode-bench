#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/energy-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiEnergyConsumption");

static double g_node0Initial = 0.0, g_node1Initial = 0.0;
static double g_node0Consumed = 0.0, g_node1Consumed = 0.0;

void RemainingEnergyTrace0(double oldValue, double remainingEnergy)
{
    g_node0Consumed = g_node0Initial - remainingEnergy;
    NS_LOG_INFO("Time " << Simulator::Now().GetSeconds() 
        << "s: Node0 RemainingEnergy: " << remainingEnergy << " J, Consumed=" << g_node0Consumed << " J");
}
void RemainingEnergyTrace1(double oldValue, double remainingEnergy)
{
    g_node1Consumed = g_node1Initial - remainingEnergy;
    NS_LOG_INFO("Time " << Simulator::Now().GetSeconds() 
        << "s: Node1 RemainingEnergy: " << remainingEnergy << " J, Consumed=" << g_node1Consumed << " J");
}
void TxTrace(Ptr<const Packet> packet)
{
    NS_LOG_INFO("Time " << Simulator::Now().GetSeconds() << "s: Packet transmitted, size=" << packet->GetSize() << " bytes");
}
void RxTrace(Ptr<const Packet> packet, const Address &address)
{
    NS_LOG_INFO("Time " << Simulator::Now().GetSeconds() << "s: Packet received, size=" << packet->GetSize() << " bytes");
}
int main(int argc, char *argv[])
{
    double simulationTime = 5.0;
    uint32_t packetSize = 512;
    double startTime = 1.0;
    double nodeDistance = 10.0;
    double energyBoundJ = 0.1;

    CommandLine cmd;
    cmd.AddValue("packetSize", "UDP packet size in bytes", packetSize);
    cmd.AddValue("startTime", "Time to start packet transmission", startTime);
    cmd.AddValue("distance", "Distance between nodes (meters)", nodeDistance);
    cmd.AddValue("simulationTime", "Total simulation time (seconds)", simulationTime);
    cmd.AddValue("energyBoundJ", "Maximum permitted node energy consumption (J)", energyBoundJ);
    cmd.Parse(argc, argv);

    NS_LOG_INFO("Building topology...");
    NodeContainer nodes;
    nodes.Create(2);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211b);
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    phy.SetChannel(channel.Create());
    WifiMacHelper mac;
    Ssid ssid = Ssid("ns3-energy");

    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevice = wifi.Install(phy, mac, nodes.Get(0));
    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, nodes.Get(1));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue(0.0),
                                 "MinY", DoubleValue(0.0),
                                 "DeltaX", DoubleValue(nodeDistance),
                                 "DeltaY", DoubleValue(0.0),
                                 "GridWidth", UintegerValue(2),
                                 "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(NetDeviceContainer(staDevice, apDevice));

    // ENERGY MODEL
    BasicEnergySourceHelper energySourceHelper;
    energySourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(1.0));
    EnergySourceContainer energySources = energySourceHelper.Install(nodes);

    WifiRadioEnergyModelHelper radioEnergyHelper;
    DeviceEnergyModelContainer deviceModels;
    deviceModels.Add(radioEnergyHelper.Install(staDevice.Get(0), energySources.Get(0)));
    deviceModels.Add(radioEnergyHelper.Install(apDevice.Get(0), energySources.Get(1)));

    Ptr<BasicEnergySource> source0 = DynamicCast<BasicEnergySource>(energySources.Get(0));
    Ptr<BasicEnergySource> source1 = DynamicCast<BasicEnergySource>(energySources.Get(1));
    g_node0Initial = source0->GetInitialEnergy();
    g_node1Initial = source1->GetInitialEnergy();

    source0->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergyTrace0));
    source1->TraceConnectWithoutContext("RemainingEnergy", MakeCallback(&RemainingEnergyTrace1));

    // TRAFFIC: Node0 --> Node1 (UDP)
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(interfaces.GetAddress(1), port)));
    onoff.SetConstantRate(DataRate("1Mbps"), packetSize);
    onoff.SetAttribute("StartTime", TimeValue(Seconds(startTime)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(simulationTime)));
    ApplicationContainer apps = onoff.Install(nodes.Get(0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer appsink = sink.Install(nodes.Get(1));

    // TRACE PACKET TX/RX
    phy.EnablePcapAll("wifi-energy");
    staDevice.Get(0)->TraceConnectWithoutContext("PhyTxEnd", MakeCallback(&TxTrace));
    apDevice.Get(0)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxTrace));

    NS_LOG_INFO("Starting simulation for " << simulationTime << " seconds...");
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    double node0Remaining = source0->GetRemainingEnergy();
    double node1Remaining = source1->GetRemainingEnergy();

    std::cout << "Node0 initial: " << g_node0Initial << " J, remaining: " << node0Remaining
              << " J, consumed: " << g_node0Initial - node0Remaining << " J" << std::endl;
    std::cout << "Node1 initial: " << g_node1Initial << " J, remaining: " << node1Remaining
              << " J, consumed: " << g_node1Initial - node1Remaining << " J" << std::endl;

    if ((g_node0Initial - node0Remaining) > energyBoundJ)
        std::cout << "Warning: Node0 consumed more than " << energyBoundJ << " J!" << std::endl;
    if ((g_node1Initial - node1Remaining) > energyBoundJ)
        std::cout << "Warning: Node1 consumed more than " << energyBoundJ << " J!" << std::endl;

    Simulator::Destroy();
    return 0;
}