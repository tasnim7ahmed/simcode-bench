#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/dsdv-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VANETDSDVSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 10;
    double simTime = 60.0;
    double distance = 500.0; // meters

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of nodes in the simulation", numNodes);
    cmd.AddValue("simTime", "Total simulation time (seconds)", simTime);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numNodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance / numNodes),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(numNodes),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    for (NodeContainer::Iterator it = nodes.Begin(); it != nodes.End(); ++it) {
        Ptr<Node> node = (*it);
        Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
        Vector position = mob->GetPosition();
        NS_LOG_DEBUG("Node initial position: " << position.x);
        mob->SetVelocity(Vector(20.0, 0.0, 0.0)); // Moving along X-axis at 20 m/s
    }

    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue("OfdmRate6Mbps"),
                                 "ControlMode", StringValue("OfdmRate6Mbps"));

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    InternetStackHelper internet;
    dsdv::DsdvHelper dsdv;
    internet.SetRoutingHelper(dsdv);
    internet.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(interfaces.GetAddress(numNodes - 1), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer apps;
    for (uint32_t i = 0; i < numNodes - 1; ++i) {
        apps = onoff.Install(nodes.Get(i));
        apps.Start(Seconds(1.0));
        apps.Stop(Seconds(simTime - 1));
    }

    UdpServerHelper server(port);
    apps = server.Install(nodes.Get(numNodes - 1));
    apps.Start(Seconds(0.0));
    apps.Stop(Seconds(simTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    AsciiTraceHelper ascii;
    wifiPhy.EnableAsciiAll(ascii.CreateFileStream("vanet-dsdv.tr"));
    wifiPhy.EnablePcapAll("vanet-dsdv");

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (auto iter = stats.begin(); iter != stats.end(); ++iter) {
        NS_LOG_UNCOND("Flow ID: " << iter->first << " Tx Packets = " << iter->second.txPackets << " Rx Packets = " << iter->second.rxPackets);
    }

    Simulator::Destroy();
    return 0;
}