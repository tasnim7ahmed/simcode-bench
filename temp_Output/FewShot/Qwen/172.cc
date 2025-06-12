#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Simulation time
    double simTime = 30.0;
    uint32_t numNodes = 10;
    std::string phyMode("DsssRate1Mbps");

    // Create nodes
    NodeContainer nodes;
    nodes.Create(numNodes);

    // Setup wireless channel and PHY
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    // Setup MAC layer (Ad-hoc mode)
    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    WifiHelper wifi;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(phyMode), "ControlMode", StringValue(phyMode));
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    // Mobility model: Random movement within 500x500 area
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(100.0),
                                  "DeltaY", DoubleValue(100.0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 500, 0, 500)));
    mobility.Install(nodes);

    // Install internet stack with AODV routing
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // UDP traffic setup
    uint16_t port = 9;
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinks;

    for (uint32_t i = 0; i < numNodes; ++i) {
        ApplicationContainer sinkApp = sinkHelper.Install(nodes.Get(i));
        sinkApp.Start(Seconds(0.0));
        sinks.Add(sinkApp);
    }

    // Generate random traffic flows
    Ptr<UniformRandomVariable> startTimeRng = CreateObject<UniformRandomVariable>();
    startTimeRng->SetAttribute("Min", DoubleValue(1.0));
    startTimeRng->SetAttribute("Max", DoubleValue(simTime - 5.0));

    Ptr<UniformRandomVariable> nodeRng = CreateObject<UniformRandomVariable>();
    nodeRng->SetAttribute("Min", DoubleValue(0));
    nodeRng->SetAttribute("Max", DoubleValue(numNodes - 1));

    for (uint32_t i = 0; i < numNodes; ++i) {
        uint32_t fromNode = i;
        uint32_t toNode;
        do {
            toNode = static_cast<uint32_t>(nodeRng->GetValue());
        } while (toNode == fromNode);

        InetSocketAddress destAddr(interfaces.GetAddress(toNode), port);
        OnOffHelper onoff("ns3::UdpSocketFactory", destAddr);
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
        onoff.SetAttribute("PacketSize", UintegerValue(512));

        ApplicationContainer app = onoff.Install(nodes.Get(fromNode));
        double startTime = startTimeRng->GetValue();
        app.Start(Seconds(startTime));
        app.Stop(Seconds(startTime + 5.0));
    }

    // Enable pcap tracing
    wifiPhy.EnablePcapAll("adhoc_wireless_network");

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Calculate metrics
    double totalRx = 0;
    double totalTx = 0;
    double totalDelay = 0;

    for (uint32_t i = 0; i < sinks.GetN(); ++i) {
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(sinks.Get(i));
        totalRx += sink->GetTotalReceivedBytes();
        totalTx += sink->GetTotalSentBytes();
        if (sink->GetTotalReceivedBytes() > 0) {
            totalDelay += sink->GetAverageRtt().ToDouble(Time::S);
        }
    }

    double pdr = (totalTx > 0) ? (totalRx / totalTx) : 0;
    double avgDelay = (totalRx > 0) ? (totalDelay / numNodes) : 0;

    std::cout.precision(3);
    std::cout << "Packet Delivery Ratio: " << std::fixed << pdr * 100 << "%" << std::endl;
    std::cout << "Average End-to-End Delay: " << std::fixed << avgDelay << "s" << std::endl;

    Simulator::Destroy();
    return 0;
}