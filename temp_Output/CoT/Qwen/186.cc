#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetSimulation");

int main(int argc, char *argv[]) {
    uint32_t nVehicles = 5;
    double simDuration = 10.0;
    double dataRate = 24.0; // Mbps
    uint16_t port = 8080;
    double bsmInterval = 1.0; // seconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("nVehicles", "Number of vehicles in the simulation", nVehicles);
    cmd.AddValue("simDuration", "Duration of simulation in seconds", simDuration);
    cmd.AddValue("dataRate", "Data rate for BSM transmission (Mbps)", dataRate);
    cmd.AddValue("bsmInterval", "BSM transmission interval in seconds", bsmInterval);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(999999));
    Config::SetDefault("ns3::WifiRemoteStationManager::FragmentationThreshold", UintegerValue(999999));

    NodeContainer nodes;
    nodes.Create(nVehicles);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                  "DataMode", StringValue("OfdmRate24Mbps"),
                                  "ControlMode", StringValue("OfdmRate24Mbps"));

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    for (NodeContainer::Iterator it = nodes.Begin(); it != nodes.End(); ++it) {
        Ptr<Node> node = *it;
        Ptr<MobilityModel> mob = node->GetObject<MobilityModel>();
        Vector position = Vector(100.0 * (it - nodes.Begin()), 0, 0);
        mob->SetPosition(position);
        mob->SetAttribute("Velocity", RandomVariableStreamValue(ConstantVariableValue(20.0))); // m/s
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    ApplicationContainer apps;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(256));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate(dataRate * 1e6)));

    for (uint32_t i = 0; i < nVehicles; ++i) {
        for (uint32_t j = 0; j < nVehicles; ++j) {
            if (i != j) {
                UdpEchoServerHelper echoServer(port);
                ApplicationContainer serverApps = echoServer.Install(nodes.Get(j));
                serverApps.Start(Seconds(0.0));
                serverApps.Stop(Seconds(simDuration));

                onoff.SetAttribute("Remote", AddressValue(Address(interfaces.GetAddress(j), port)));
                ApplicationContainer clientApps = onoff.Install(nodes.Get(i));
                clientApps.Start(Seconds(0.1 + i * bsmInterval));
                clientApps.Stop(Seconds(simDuration - 0.1));
            }
        }
    }

    AsciiTraceHelper ascii;
    wifiPhy.EnableAsciiAll(ascii.CreateFileStream("vanet-simulation.tr"));
    wifiPhy.EnablePcapAll("vanet-simulation");

    AnimationInterface anim("vanet-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.5));

    Simulator::Stop(Seconds(simDuration));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}