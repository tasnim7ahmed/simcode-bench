#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/aodv-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time and mobility trace file
    double totalTime = 10.0;
    std::string phyMode("DsssRate1Mbps");
    std::string traceFile = "manet-aodv.mobility.trace";

    // Enable AODV logging
    LogComponentEnable("AodvRoutingProtocol", LOG_LEVEL_ALL);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes (5 in total)
    NodeContainer nodes;
    nodes.Create(5);

    // Setup Mobility Model - RandomWaypoint for each node
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=2.0]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.2]"),
                              "PositionAllocator", PointerValue(mobility.GetPositionAllocator()));
    mobility.Install(nodes);

    // Enable ASCII tracing of mobility
    AsciiTraceHelper ascii;
    MobilityModel::EnableAsciiAll(ascii.CreateFileStream(traceFile));

    // Setup WiFi
    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    wifiPhy.Set("ChannelNumber", UintegerValue(1));
    wifiPhy.Set("TxPowerStart", DoubleValue(5.0));
    wifiPhy.Set("TxPowerEnd", DoubleValue(5.0));
    wifiPhy.Set("TxPowerLevels", UintegerValue(1));
    wifiPhy.Set("RxGain", DoubleValue(0));
    wifiPhy.Set("CcaMode1Threshold", DoubleValue(-79.0));

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifiPhy.Install(wifiMac, nodes);

    // Install Internet Stack with AODV
    AodvHelper aodv;
    Ipv4ListRoutingHelper listRH;
    listRH.Add(aodv, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRH);
    internet.Install(nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Set up UDP Echo Server on node 4
    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);
    ApplicationContainer serverApp = echoServer.Install(nodes.Get(4));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(totalTime));

    // Set up UDP Echo Client on node 0
    UdpEchoClientHelper echoClient(interfaces.GetAddress(4), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(20));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApp = echoClient.Install(nodes.Get(0));
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(totalTime));

    // Run the simulation
    Simulator::Stop(Seconds(totalTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}