#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/netanim-module.h"

int main(int argc, char *argv[])
{
    double simulationTime = 60.0;
    double vehicleSpeed = 20.0;
    double initialDistance = 200.0;
    uint32_t packetSize = 1000;
    double sendInterval = 0.5;

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total simulation time in seconds", simulationTime);
    cmd.AddValue("vehicleSpeed", "Speed of vehicles in m/s", vehicleSpeed);
    cmd.AddValue("initialDistance", "Initial distance between vehicles in meters", initialDistance);
    cmd.AddValue("packetSize", "Size of UDP packets in bytes", packetSize);
    cmd.AddValue("sendInterval", "Time interval between UDP packet sends", sendInterval);
    cmd.Parse(argc, argv);

    ns3::NodeContainer vehicles;
    vehicles.Create(2);

    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(vehicles);

    vehicles.Get(0)->GetObject<ns3::ConstantVelocityMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    vehicles.Get(0)->GetObject<ns3::ConstantVelocityMobilityModel>()->SetVelocity(ns3::Vector(vehicleSpeed, 0.0, 0.0));

    vehicles.Get(1)->GetObject<ns3::ConstantVelocityMobilityModel>()->SetPosition(ns3::Vector(initialDistance, 0.0, 0.0));
    vehicles.Get(1)->GetObject<ns3::ConstantVelocityMobilityModel>()->SetVelocity(ns3::Vector(-vehicleSpeed, 0.0, 0.0));

    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_PHY_STANDARD_80211p);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", ns3::StringValue("OfdmRate6Mbps"));

    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::LogDistancePropagationLossModel");
    channel.SetPropagationDelayModel("ns3::RandomPropagationDelayModel");
    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    ns3::WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");

    ns3::NetDeviceContainer wifiDevices;
    wifiDevices = wifi.Install(phy, mac, vehicles);

    ns3::InternetStackHelper internet;
    internet.Install(vehicles);

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer ipv4Interfaces;
    ipv4Interfaces = ipv4.Assign(wifiDevices);

    uint16_t port = 9;

    ns3::UdpEchoServerHelper echoServer(port);
    ns3::ApplicationContainer serverApps = echoServer.Install(vehicles.Get(0));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(simulationTime - 1.0));

    ns3::UdpEchoClientHelper echoClient(ipv4Interfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", ns3::UintegerValue(4294967295U));
    echoClient.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(sendInterval)));
    echoClient.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));
    ns3::ApplicationContainer clientApps = echoClient.Install(vehicles.Get(1));
    clientApps.Start(ns3::Seconds(1.0));
    clientApps.Stop(ns3::Seconds(simulationTime - 1.0));

    ns3::NetAnimHelper netanim;
    netanim.SetOutputFileName("vanet-animation.xml");
    netanim.EnablePacketMetadata();
    netanim.EnableWifiMacTraces();
    netanim.EnableMobility();

    ns3::Simulator::Stop(ns3::Seconds(simulationTime));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}