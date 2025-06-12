#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VehicularNetworkSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(4);

    WifiMacHelper wifiMac;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(20.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
    mobility.Install(nodes);

    for (auto nodeIt = nodes.Begin(); nodeIt != nodes.End(); ++nodeIt) {
        Ptr<Node> node = *nodeIt;
        Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
        mobility->SetAttribute("Velocity", Vector3DValue(Vector(10.0, 0.0, 0.0)));
    }

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetBroadcast(), port)));
    onoff.SetConstantRate(DataRate("1kbps"));
    onoff.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer apps;

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        for (uint32_t j = 0; j < nodes.GetN(); ++j) {
            if (i != j) {
                Address destAddr = InetSocketAddress(interfaces.GetAddress(j), port);
                OnOffHelper helper("ns3::UdpSocketFactory", destAddr);
                helper.SetConstantRate(DataRate("1kbps"));
                helper.SetAttribute("PacketSize", UintegerValue(512));
                helper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
                helper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

                apps = helper.Install(nodes.Get(i));
                apps.Start(Seconds(1.0));
                apps.Stop(Seconds(10.0));
            }
        }
    }

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        apps = sink.Install(nodes.Get(i));
        apps.Start(Seconds(0.0));
    }

    AsciiTraceHelper ascii;
    wifi.EnableAsciiAll(ascii.CreateFileStream("vehicular-network.tr"));
    wifi.EnablePcapAll("vehicular-network");

    AnimationInterface anim("vehicular-animation.xml");
    anim.SetMobilityPollInterval(Seconds(0.1));

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}