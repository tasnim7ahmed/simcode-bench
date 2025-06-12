#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ManetOlsrSimulation");

int main(int argc, char *argv[]) {
    uint32_t numNodes = 6;
    double totalTime = 10.0;
    double areaSize = 100.0;
    double minSpeed = 5.0;
    double maxSpeed = 15.0;

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(100));

    NodeContainer nodes;
    nodes.Create(numNodes);

    WifiMacHelper wifiMac;
    WifiPhyHelper wifiPhy;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    wifi.SetRemoteStationManager("ns3::ArfWifiManager");

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaSize) + "]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=" + std::to_string(areaSize) + "]"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::UniformRandomVariable[Min=" + std::to_string(minSpeed) + "|Max=" + std::to_string(maxSpeed) + "]"),
                              "Pause", TimeValue(Seconds(0.0)));
    mobility.Install(nodes);

    InternetStackHelper stack;
    OlsrHelper olsr;
    stack.SetRoutingHelper(olsr);
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t port = 9;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(numNodes - 1), port));
    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(numNodes - 1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(totalTime));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetConstantRate(DataRate("1kbps"), 500);
    onoff.SetAttribute("StartTime", TimeValue(Seconds(1.0)));
    onoff.SetAttribute("StopTime", TimeValue(Seconds(totalTime - 1.0)));

    ApplicationContainer sourceApps = onoff.Install(nodes.Get(0));
    sourceApps.Start(Seconds(1.0));
    sourceApps.Stop(Seconds(totalTime - 1.0));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Simulator::Stop(Seconds(totalTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}