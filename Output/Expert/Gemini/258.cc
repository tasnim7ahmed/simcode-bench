#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/on-off-application.h"
#include "ns3/udp-echo-client.h"
#include "ns3/udp-echo-server.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Config::SetDefault ("ns3::Wifi80211pMac::BE_CwMin", UintegerValue (31));
    Config::SetDefault ("ns3::Wifi80211pMac::BE_CwMax", UintegerValue (1023));
    Config::SetDefault ("ns3::Wifi80211pMac::BE_Aifs", UintegerValue (9));
    Config::SetDefault ("ns3::Wifi80211pMac::Slot", TimeValue (Seconds (0.00002)));

    NodeContainer nodes;
    nodes.Create(3);

    MobilityHelper mobility;
    mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (50.0),
                                 "DeltaY", DoubleValue (0.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    Ptr<ConstantVelocityMobilityModel> mob0 = nodes.Get(0)->GetObject<ConstantVelocityMobilityModel>();
    mob0->SetVelocity(Vector(10,0,0));
    Ptr<ConstantVelocityMobilityModel> mob1 = nodes.Get(1)->GetObject<ConstantVelocityMobilityModel>();
    mob1->SetVelocity(Vector(15,0,0));
    Ptr<ConstantVelocityMobilityModel> mob2 = nodes.Get(2)->GetObject<ConstantVelocityMobilityModel>();
    mob2->SetVelocity(Vector(5,0,0));

    YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Create();
    wifiPhy.SetChannel(wifiChannel.Create());
    QosWaveMacHelper wifi80211pMac = QosWaveMacHelper::Default();
    Wifi80211pHelper wifi80211p;
    NetDeviceContainer devices = wifi80211p.Install(wifiPhy, wifi80211pMac, nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    UdpEchoServerHelper echoServer (9);
    ApplicationContainer serverApps = echoServer.Install (nodes.Get (2));
    serverApps.Start (Seconds (1.0));
    serverApps.Stop (Seconds (10.0));

    UdpEchoClientHelper echoClient (interfaces.GetAddress (2), 9);
    echoClient.SetAttribute ("MaxPackets", UintegerValue (3));
    echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
    echoClient.SetAttribute ("PacketSize", UintegerValue (512));
    ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
    clientApps.Start (Seconds (2.0));
    clientApps.Stop (Seconds (10.0));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}