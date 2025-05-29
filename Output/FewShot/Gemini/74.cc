#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/aodv-module.h"
#include "ns3/dsr-module.h"
#include "ns3/netanim-module.h"
#include "ns3/flow-monitor-module.h"
#include <iostream>
#include <fstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetRoutingComparison");

enum routingProtocol {
    DSDV,
    AODV,
    OLSR,
    DSR
};

std::string protocolName[] = {"DSDV", "AODV", "OLSR", "DSR"};

int main(int argc, char *argv[]) {
    bool enablePcap = false;
    bool enableFlowMonitor = false;
    bool enableMobilityTrace = false;
    routingProtocol routing = AODV;
    int numNodes = 50;
    double simulationTime = 200;
    double maxSpeed = 20;
    double txPowerDbm = 7.5;

    CommandLine cmd;
    cmd.AddValue("pcap", "Enable PCAP tracing", enablePcap);
    cmd.AddValue("flowMonitor", "Enable Flow Monitor", enableFlowMonitor);
    cmd.AddValue("mobilityTrace", "Enable Mobility Trace", enableMobilityTrace);
    cmd.AddValue("routingProtocol", "Routing protocol [DSDV|AODV|OLSR|DSR]", routing);
    cmd.AddValue("numNodes", "Number of nodes", numNodes);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("maxSpeed", "Maximum speed of nodes in m/s", maxSpeed);
    cmd.AddValue("txPower", "Wifi transmit power in dBm", txPowerDbm);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=1500.0]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                               "Speed", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(maxSpeed) + "]"),
                               "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.0]");
    mobility.Install(nodes);

    WifiHelper wifi;
    WifiMacHelper wifiMac;
    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());

    wifiPhy.SetTxPowerDbm(txPowerDbm);

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    InternetStackHelper internet;
    OlsrHelper olsr;
    DsdvHelper dsdv;
    AodvHelper aodv;
    DsrHelper dsr;
    DsrMainHelper dsrMain;

    switch (routing) {
        case DSDV:
            internet.Add(dsdv, Ipv4RoutingHelper::Null);
            break;
        case AODV:
            internet.Add(aodv, Ipv4RoutingHelper::Null);
            break;
        case OLSR:
            internet.Add(olsr, Ipv4RoutingHelper::Null);
            break;
        case DSR:
            internet.Add(dsrMain, Ipv4RoutingHelper::Null);
            break;
    }
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9;
    ApplicationContainer sinkApps;

    for (int i = 0; i < 10; ++i) {
        UdpServerHelper sink(port);
        sinkApps.Add(sink.Install(nodes.Get(i)));
        sinkApps.Start(Seconds(0.0));
        sinkApps.Stop(Seconds(simulationTime));
    }

    ApplicationContainer clientApps;
    for (int i = 0; i < 10; ++i) {
        UdpClientHelper client(interfaces.GetAddress(i), port);
        client.SetAttribute("MaxPackets", UintegerValue(4294967295));
        client.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
        client.SetAttribute("PacketSize", UintegerValue(1024));
        clientApps.Add(client.Install(nodes.Get(i + 10)));
        clientApps.Start(Seconds(50.0 + (double)rand() / RAND_MAX));
        clientApps.Stop(Seconds(simulationTime));
    }

    if (enablePcap) {
        wifiPhy.EnablePcapAll("manet");
    }

    FlowMonitorHelper flowmon;
    if (enableFlowMonitor) {
        flowmon.InstallAll();
    }

    if (enableMobilityTrace) {
        std::ofstream os;
        os.open("mobility.trace");
        mobility.EnableAsciiAll(os);
        os.close();
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    if (enableFlowMonitor) {
        flowmon.SerializeToXmlFile("manet.flowmon", false, true);
    }

    std::ofstream output ("results.csv");
    output << "Protocol,Nodes,Speed,TxPower,PacketReceived,PacketLost,BytesReceived" << std::endl;

    uint64_t totalPacketsReceived = 0;
    uint64_t totalPacketsLost = 0;
    uint64_t totalBytesReceived = 0;

    if(enableFlowMonitor){
        Ptr<FlowMonitor> monitor = flowmon.GetMonitor();
        for (const auto& flowEntry : monitor->GetFlows()) {
            FlowMonitor::FlowStats stats = monitor->GetFlowStats(flowEntry.first);
            totalPacketsReceived += stats.rxPackets;
            totalPacketsLost += stats.lostPackets;
            totalBytesReceived += stats.rxBytes;
        }
    }

    output << protocolName[routing] << ","
            << numNodes << ","
            << maxSpeed << ","
            << txPowerDbm << ","
            << totalPacketsReceived << ","
            << totalPacketsLost << ","
            << totalBytesReceived << std::endl;

    output.close();

    Simulator::Destroy();
    return 0;
}