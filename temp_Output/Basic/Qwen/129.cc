#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/vlan-helper.h"
#include "ns3/dhcp-server-helper.h"
#include "ns3/dhcp-client-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VlanDhcpInterVlanRouting");

int main(int argc, char *argv[]) {
    bool tracing = true;
    uint32_t nHostsPerVlan = 2;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tracing", "Enable pcap tracing", tracing);
    cmd.AddValue("nHostsPerVlan", "Number of hosts per VLAN", nHostsPerVlan);
    cmd.Parse(argc, argv);

    // Create nodes
    NodeContainer hosts[3];
    for (auto &host : hosts) {
        host.Create(nHostsPerVlan);
    }

    NodeContainer switchNode;
    switchNode.Create(1);

    NodeContainer router;
    router.Create(1);

    // Create links
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Connect hosts to switch with VLANs
    NetDeviceContainer hostSwitchDevices[3];
    VlanHelper vlanHelper;

    for (uint8_t i = 0; i < 3; ++i) {
        hostSwitchDevices[i] = csma.Install(NodeContainer(hosts[i], switchNode));
        for (uint32_t j = 0; j < hostSwitchDevices[i].GetN(); ++j) {
            if (j == 0) continue; // Skip the switch device for now
            vlanHelper.SetVlanId(hostSwitchDevices[i].Get(j), i + 1);
        }
    }

    // Connect switch to router
    NetDeviceContainer switchRouterDevices = p2p.Install(switchNode, router.Get(0));

    // Install Internet stack
    InternetStackHelper internet;
    for (auto &host : hosts) {
        internet.Install(host);
    }
    internet.Install(router);

    // Assign IP addresses via DHCP
    DhcpServerHelper dhcpServer(router.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());

    for (uint8_t i = 0; i < 3; ++i) {
        std::ostringstream subnet;
        subnet << "192.168." << i << ".0";
        Ipv4Address network(subnet.str().c_str());
        Ipv4Mask mask("255.255.255.0");
        dhcpServer.AddNetwork(network, mask, Seconds(10), Seconds(20));
    }

    ApplicationContainer dhcpApps = dhcpServer.Install(router);
    dhcpApps.Start(Seconds(1.0));
    dhcpApps.Stop(Seconds(20.0));

    DhcpClientHelper dhcpClient;
    ApplicationContainer clientApps;

    Ipv4InterfaceContainer hostInterfaces[3];

    for (uint8_t i = 0; i < 3; ++i) {
        for (uint32_t j = 0; j < hosts[i].GetN(); ++j) {
            clientApps.Add(dhcpClient.Install(hosts[i].Get(j)));
        }
    }

    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(20.0));

    // Manually assign router interface
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer routerInterface = ipv4.Assign(switchRouterDevices);

    // Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup traffic: Ping between VLANs
    V4PingHelper ping(router->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal());
    ping.SetAttribute("Verbose", BooleanValue(true));
    for (uint8_t i = 0; i < 3; ++i) {
        for (uint32_t j = 0; j < hosts[i].GetN(); ++j) {
            ping.SetRemote(Ipv4Address(("192.168." + std::to_string((i + 1) % 3) + ".2").c_str()));
            ApplicationContainer apps = ping.Install(hosts[i].Get(j));
            apps.Start(Seconds(5.0));
            apps.Stop(Seconds(15.0));
        }
    }

    // Pcap tracing
    if (tracing) {
        csma.EnablePcapAll("vlan-dhcp-inter-vlan-routing", false);
        p2p.EnablePcapAll("vlan-dhcp-inter-vlan-routing", false);
    }

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}