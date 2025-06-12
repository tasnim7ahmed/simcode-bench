#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-routing-helper.h"
#include "ns3/6lowpan-helper.h"
#include "ns3/lr-wpan-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("IoT6LoWPANSimulation");

int main(int argc, char *argv[]) {
    uint32_t numDevices = 5;
    Time simulationTime = Seconds(10.0);
    Time interval = Seconds(2.0);

    CommandLine cmd;
    cmd.AddValue("numDevices", "Number of IoT devices", numDevices);
    cmd.AddValue("simulationTime", "Total simulation time", simulationTime);
    cmd.AddValue("interval", "Packet sending interval", interval);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(numDevices + 1); // +1 for the central server

    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrWpanDevices = lrWpanHelper.Install(nodes);

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0),
                                  "GridWidth", UintegerValue(5),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(lrWpanDevices);
    interfaces.SetForwarding(1, true); // Server node forwards

    SixLowPanHelper sixlowpan;
    NetDeviceContainer sixlowpanDevices = sixlowpan.Install(lrWpanDevices);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(5)));

    NetDeviceContainer p2pDevices;
    p2pDevices = p2p.Install(nodes.Get(0), nodes.Get(numDevices)); // Connect first IoT device to server

    Ipv6AddressHelper p2pIpv6;
    p2pIpv6.SetBase(Ipv6Address("2001:2::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer p2pInterfaces = p2pIpv6.Assign(p2pDevices);
    p2pInterfaces.SetForwarding(0, true); // IoT node forwards

    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6RoutingProtocol> serverRouting = routingHelper.Create(nodes.Get(numDevices));
    nodes.Get(numDevices)->GetObject<Ipv6>()->SetRoutingProtocol(serverRouting);

    for (uint32_t i = 0; i < numDevices; ++i) {
        Ptr<Ipv6StaticRouting> routing = routingHelper.GetStaticRouting(nodes.Get(i)->GetObject<Ipv6>());
        if (routing) {
            routing->AddNetworkRouteTo("2001:2::", Ipv6Prefix(64), 2, 1); // Send to server via p2p link
        }
    }

    UdpServerHelper server(9);
    ApplicationContainer serverApp = server.Install(nodes.Get(numDevices));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(simulationTime);

    UdpClientHelper client(interfaces.GetAddress(numDevices, 1), 9);
    client.SetAttribute("MaxPackets", UintegerValue(1000000));
    client.SetAttribute("Interval", TimeValue(interval));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < numDevices; ++i) {
        clientApps.Add(client.Install(nodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(simulationTime);

    AsciiTraceHelper ascii;
    lrWpanHelper.EnableAsciiAll(ascii.CreateFileStream("iot-6lowpan.tr"));
    lrWpanHelper.EnablePcapAll("iot-6lowpan");

    AnimationInterface anim("iot-6lowpan.xml");
    anim.SetConstantPosition(nodes.Get(numDevices), 50.0, 0.0);

    Simulator::Stop(simulationTime);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}