#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/wave-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VanetWifiSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(3);

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_OFC_PRETTIES_HACK);

    Wifi80211pMacHelper wifiMac = Wifi80211pMacHelper();
    Wifi80211pHelper wifi = Wifi80211pHelper();
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("OfdmRate6MbpsBW10MHz"), "ControlMode", StringValue("OfdmRate6MbpsBW10MHz"));

    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    // Set positions and velocities
    Ptr<ConstantVelocityMobilityModel> node0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    node0->SetPosition(Vector(0.0, 0.0, 0.0));
    node0->SetVelocity(Vector(20.0, 0.0, 0.0)); // 20 m/s along X-axis

    Ptr<ConstantVelocityMobilityModel> node1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    node1->SetPosition(Vector(100.0, 0.0, 0.0));
    node1->SetVelocity(Vector(15.0, 0.0, 0.0)); // 15 m/s along X-axis

    Ptr<ConstantVelocityMobilityModel> node2 = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();
    node2->SetPosition(Vector(200.0, 0.0, 0.0));
    node2->SetVelocity(Vector(10.0, 0.0, 0.0)); // 10 m/s along X-axis

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(2));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(2), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(3));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}