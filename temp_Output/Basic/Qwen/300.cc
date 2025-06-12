#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/zigbee-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ZigBeeSensorNetwork");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(10);

    Ptr<ZigbeeHelper> zigbee = CreateObject<ZigbeeHelper>();
    zigbee->SetMode("PRO");
    zigbee->AssignPanId(nodes);

    NetDeviceContainer devices;
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        devices.Add(zigbee->Install(nodes.Get(i)));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(interfaces.GetAddress(0), sinkPort));

    PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(nodes.Get(0));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    OnOffHelper onoff("ns3::UdpSocketFactory", sinkAddress);
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("1kbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(128));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=500ms]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0ms]"));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        clientApps.Add(onoff.Install(nodes.Get(i)));
    }

    clientApps.Start(Seconds(0.5));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("zigbee-wsn.tr");
    zigbee->EnableAsciiAll(stream);

    zigbee->EnablePcapAll("zigbee-wsn");

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}