#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-routing-table-entry.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VlanRoutingSimulation");

int main(int argc, char *argv[]) {
    bool tracing = true;
    uint32_t packetSize = 1024;
    Time interPacketInterval = Seconds(1.0);
    double simulationTime = 10.0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer hub;
    hub.Create(1);

    NodeContainer vlanNodes;
    vlanNodes.Create(3);

    // Connect each VLAN node to the hub via CSMA
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate(100e6)));
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(100)));

    NetDeviceContainer devicesHub[3];
    NetDeviceContainer devicesVlan[3];

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer interfacesHub[3];
    Ipv4InterfaceContainer interfacesVlan[3];

    for (uint8_t i = 0; i < 3; ++i) {
        std::ostringstream subnet;
        subnet << "192.168." << static_cast<uint16_t>(i + 1) << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");

        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));

        // Create a point-to-point link between hub and each VLAN node
        NetDeviceContainer ndc = p2p.Install(hub.Get(0), vlanNodes.Get(i));
        devicesHub[i] = NetDeviceContainer(ndc.Get(0));
        devicesVlan[i] = NetDeviceContainer(ndc.Get(1));

        interfacesHub[i] = address.Assign(devicesHub[i]);
        interfacesVlan[i] = address.Assign(devicesVlan[i]);

        address.NewNetwork();
    }

    // Enable IPv4 routing on the hub node
    InternetStackHelper internet;
    internet.SetRoutingHelper(Ipv4StaticRoutingHelper()); // Use static routing
    internet.Install(hub);

    InternetStackHelper internetVlan;
    internetVlan.Install(vlanNodes);

    // Manually set up routing tables on the hub
    Ptr<Ipv4> ipv4Hub = hub.Get(0)->GetObject<Ipv4>();
    Ipv4StaticRoutingHelper routingHelper;

    for (uint8_t i = 0; i < 3; ++i) {
        for (uint8_t j = 0; j < 3; ++j) {
            if (i != j) {
                Ipv4Address destNet = interfacesVlan[j].GetAddress(0);
                Ipv4Mask mask("/24");
                Ipv4Address nextHop = interfacesHub[i].GetAddress(0);
                uint32_t interfaceIdx = i;
                routingHelper.AddNetworkRouteTo(Ipv4RoutingTableEntry::CreateNetworkRouteTo(destNet, mask, nextHop, interfaceIdx), ipv4Hub);
            }
        }
    }

    // Setup applications
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(vlanNodes.Get(0));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpEchoClientHelper echoClient(interfacesVlan[0].GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(packetSize));

    ApplicationContainer clientApps = echoClient.Install(vlanNodes.Get(1));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(simulationTime));

    // Enable tracing
    if (tracing) {
        csma.EnablePcapAll("vlan_routing_simulation", false);
        for (uint8_t i = 0; i < 3; ++i) {
            p2p.EnablePcapAll("vlan_routing_simulation_p2p", false);
        }
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}