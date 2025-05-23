#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mesh-module.h"
#include "ns3/applications-module.h"

void LogMobileNodePosition(ns3::Ptr<ns3::Node> mobileNode, double logInterval)
{
    ns3::Ptr<ns3::MobilityModel> mob = mobileNode->GetObject<ns3::MobilityModel>();
    if (mob != nullptr)
    {
        ns3::Vector pos = mob->GetPosition();
        ns3::NS_LOG_INFO("Time: " << ns3::Simulator::Now().GetSeconds() << "s, Mobile Node Position: " << pos);
    }
    
    if (ns3::Simulator::Now().GetSeconds() < ns3::Simulator::GetStopTime().GetSeconds() - logInterval / 2.0)
    {
         ns3::Simulator::Schedule(ns3::Seconds(logInterval), &LogMobileNodePosition, mobileNode, logInterval);
    }
}

int main(int argc, char *argv[])
{
    ns3::Time::SetResolution(ns3::Time::NS);
    ns3::LogComponentEnable("UdpClient", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("PacketSink", ns3::LOG_LEVEL_INFO);
    ns3::LogComponentEnable("ConstantVelocityMobilityModel", ns3::LOG_LEVEL_INFO);

    double simulationTime = 20.0;
    double mobileNodeSpeed = 10.0;
    double mobileNodeStartX = 10.0;
    uint32_t payloadSize = 1024;
    double packetIntervalSec = 0.1;
    double positionLogInterval = 1.0;

    ns3::CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Total duration of the simulation in seconds.", simulationTime);
    cmd.AddValue("mobileNodeSpeed", "Speed of the mobile node in m/s.", mobileNodeSpeed);
    cmd.AddValue("mobileNodeStartX", "Initial X position of the mobile node in meters.", mobileNodeStartX);
    cmd.AddValue("payloadSize", "Size of UDP packets in bytes.", payloadSize);
    cmd.AddValue("packetInterval", "Time interval between UDP packets in seconds.", packetIntervalSec);
    cmd.AddValue("positionLogInterval", "Interval for logging mobile node position.", positionLogInterval);
    cmd.Parse(argc, argv);

    ns3::NodeContainer nodes;
    nodes.Create(2);
    ns3::Ptr<ns3::Node> mobileNode = nodes.Get(0);
    ns3::Ptr<ns3::Node> staticNode = nodes.Get(1);

    ns3::MobilityHelper mobility;

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staticNode);
    staticNode->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));

    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(mobileNode);
    ns3::Ptr<ns3::ConstantVelocityMobilityModel> cvmm = mobileNode->GetObject<ns3::ConstantVelocityMobilityModel>();
    cvmm->SetPosition(ns3::Vector(mobileNodeStartX, 0.0, 0.0));
    cvmm->SetVelocity(ns3::Vector(mobileNodeSpeed, 0.0, 0.0));

    ns3::Simulator::Schedule(ns3::Seconds(0.0), &LogMobileNodePosition, mobileNode, positionLogInterval);

    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211s);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", ns3::StringValue("OfdmRate6Mbps"),
                                 "ControlMode", ns3::StringValue("OfdmRate6Mbps"));

    ns3::YansWifiChannelHelper channel;
    channel.SetPropagationLossModel("ns3::LogDistancePropagationLossModel",
                                    "Exponent", ns3::DoubleValue(3.0));
    channel.AddPropagationLoss("ns3::RangePropagationLossModel",
                               "MaxRange", ns3::DoubleValue(250.0));

    ns3::YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    ns3::MeshHelper mesh;
    mesh.SetStackInstaller("ns3::dot11sStackHelper");
    ns3::NetDeviceContainer wifiDevices = mesh.Install(phy, wifi, nodes);

    ns3::InternetStackHelper internet;
    internet.Install(nodes);

    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer interfaces = ipv4.Assign(wifiDevices);

    uint16_t port = 9;
    ns3::PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                                     ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer serverApp = sinkHelper.Install(staticNode);
    serverApp.Start(ns3::Seconds(0.0));
    serverApp.Stop(ns3::Seconds(simulationTime + 1.0));

    ns3::UdpClientHelper clientHelper(interfaces.GetAddress(1), port);
    clientHelper.SetAttribute("MaxPackets", ns3::UintegerValue(0));
    clientHelper.SetAttribute("Interval", ns3::TimeValue(ns3::Seconds(packetIntervalSec)));
    clientHelper.SetAttribute("PacketSize", ns3::UintegerValue(payloadSize));
    ns3::ApplicationContainer clientApp = clientHelper.Install(mobileNode);
    clientApp.Start(ns3::Seconds(1.0));
    clientApp.Stop(ns3::Seconds(simulationTime));

    phy.EnablePcapAll("mobile-static-80211s-adhoc");

    ns3::Simulator::Stop(ns3::Seconds(simulationTime + 1.0));
    ns3::Simulator::Run();

    ns3::Ptr<ns3::PacketSink> sink = ns3::DynamicCast<ns3::PacketSink>(serverApp.Get(0));
    ns3::NS_LOG_INFO("Total bytes received by static node: " << sink->GetTotalRx() << " bytes");

    ns3::Simulator::Destroy();

    return 0;
}