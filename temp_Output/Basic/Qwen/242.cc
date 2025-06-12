#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/6lowpan-net-device.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("LowPanIotSimulation");

int main(int argc, char *argv[]) {
    bool verbose = true;
    uint32_t numDevices = 5;
    double simulationTime = 10.0; // seconds
    double interval = 2.0;        // seconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("numDevices", "Number of IoT devices.", numDevices);
    cmd.AddValue("interval", "Packet sending interval in seconds.", interval);
    cmd.AddValue("verbose", "Enable logging output.", verbose);
    cmd.Parse(argc, argv);

    if (verbose) {
        LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
        LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(numDevices + 1); // +1 for the central server node

    YansLrWpanPhyHelper lrWpanPhy = YansLrWpanPhyHelper::Default();
    LrWpanHelper lrWpan;
    NetDeviceContainer lrwpanDevices = lrWpan.Install(nodes);

    // Enable 6LoWPAN on all LR-WPAN devices
    Ptr<6LoWPANNetDevice> sixlowpan = CreateObject<6LoWPANNetDevice>();
    for (uint32_t i = 0; i < lrwpanDevices.GetN(); ++i) {
        lrwpanDevices.Get(i)->SetAttribute("UseMultiAddress", BooleanValue(true));
        nodes.Get(i)->AddDevice(sixlowpan);
        sixlowpan->SetNode(nodes.Get(i));
        sixlowpan->SetNetDevice(Names::Find<NetDevice>("lrWpan"));
    }

    InternetStackHelper internetv6;
    internetv6.SetIpv4StackInstall(false);
    internetv6.SetIpv6StackInstall(true);
    internetv6.Install(nodes);

    Ipv6AddressHelper ipv6;
    Ipv6InterfaceContainer interfaces;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    interfaces = ipv6.Assign(lrwpanDevices);
    interfaces.SetForwarding(0, true);
    interfaces.SetDefaultRouteInAllNodes(0);

    // Server application setup
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    // Client applications
    UdpEchoClientHelper echoClient(interfaces.GetAddress(0, 1), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue((uint32_t)(simulationTime / interval)));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(interval)));
    echoClient.SetAttribute("PacketSize", UintegerValue(64));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i <= numDevices; ++i) {
        clientApps.Add(echoClient.Install(nodes.Get(i)));
        clientApps.Start(Seconds(2.0));
        clientApps.Stop(Seconds(simulationTime));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}