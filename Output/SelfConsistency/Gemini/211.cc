#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <iostream>
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("AdhocWifiExample");

int main(int argc, char *argv[]) {
    bool verbose = false;
    bool tracing = false;
    uint32_t packetSize = 1024;
    std::string dataRate = "5Mbps";
    std::string phyRate = "OFDM54";
    double simulationTime = 10.0;
    uint32_t numNodes = 5;

    CommandLine cmd;
    cmd.AddValue("verbose", "Tell application to log if true", verbose);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("packetSize", "Size of packet sent in bytes", packetSize);
    cmd.AddValue("dataRate", "Data rate for UDP application", dataRate);
    cmd.AddValue("phyRate", "Physical layer rate", phyRate);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("numNodes", "Number of wifi nodes", numNodes);

    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211a);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.Set("RxGain", DoubleValue(-10));
    wifiPhy.Set("TxGain", DoubleValue(10));
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    wifiPhy.SetChannel(wifiChannel.Create());

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomBoxPositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Z", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Mode", StringValue("Time"),
                              "Time", StringValue("1s"),
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    UdpServerHelper server(9);
    ApplicationContainer apps = server.Install(nodes.Get(numNodes - 1));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simulationTime));

    UdpClientHelper client(interfaces.GetAddress(numNodes - 1), 9);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295));
    client.SetAttribute("Interval", TimeValue(Time("1ms")));
    client.SetAttribute("PacketSize", UintegerValue(packetSize));
    client.SetDataRate(DataRate(dataRate));
    apps = client.Install(nodes.Get(0));
    apps.Start(Seconds(2.0));
    apps.Stop(Seconds(simulationTime));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    if (tracing) {
        wifiPhy.EnablePcapAll("adhoc");
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    monitor->CheckForLostPackets();

    std::ofstream os;
    os.open("adhoc-wifi.flowmon", std::ios::out);
    monitor->SerializeToXmlFile("adhoc-wifi.flowmon", true, true);
    os.close();

    Simulator::Destroy();

    return 0;
}