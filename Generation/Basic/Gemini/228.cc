#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/sumo-module.h"
#include "ns3/dsr-module.h"

NS_LOG_COMPONENT_DEFINE("VanetDsrSumo");

int main(int argc, char *argv[])
{
    ns3::LogComponentEnable("VanetDsrSumo", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("DsrHelper", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("DsrRouting", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("WifiPhy", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoClientApplication", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("UdpEchoServerApplication", ns3::LOG_LEVEL_INFO);

    std::string sumoConfigFile = "src/sumo/examples/data/hello_world/hello.sumo.cfg";
    uint32_t numNodes = 20;
    double simulationTime = 80.0;

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("sumoConfigFile", "SUMO configuration file", sumoConfigFile);
    cmd.AddValue("numNodes", "Number of vehicles/nodes", numNodes);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.Parse(argc, argv);

    ns3::NodeContainer nodes;
    nodes.Create(numNodes);

    ns3::SumoMobilityHelper sumoHelper;
    sumoHelper.SetAttribute("SumoConfigFile", ns3::StringValue(sumoConfigFile));
    sumoHelper.Install(nodes);

    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211p);

    ns3::YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(ns3::YansWifiChannelHelper::Create());
    wifiPhy.SetPcapDataLinkType(ns3::WifiPhyHelper::DLT_IEEE802_11_RADIO);

    ns3::WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", ns3::StringValue("OfdmRate6Mbps"),
                                 "ControlMode", ns3::StringValue("OfdmRate6Mbps"));

    ns3::NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    ns3::DsrHelper dsr;
    ns3::DsrMainHelper dsrMain;
    dsrMain.Install(dsr, nodes);

    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    uint16_t port = 9;
    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(simulationTime));

    ns3::UdpEchoClientHelper echoClient(interfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(100));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(1024));
    ns3::ApplicationContainer clientApps = echoClient.Install(nodes.Get(1));
    clientApps.Start(ns3::Seconds(2.0));
    clientApps.Stop(ns3::Seconds(simulationTime));

    wifiPhy.EnablePcap("vanet-dsr-sumo", devices);

    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}