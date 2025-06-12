#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/dhcp-server-helper.h"
#include "ns3/vlan-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VlanInterRoutingDhcpSimulation");

int main(int argc, char *argv[]) {
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("DhcpClient", LOG_LEVEL_ALL);
    LogComponentEnable("DhcpServer", LOG_LEVEL_ALL);

    // Create nodes
    NodeContainer hosts;
    hosts.Create(6);  // 2 hosts per VLAN * 3 VLANs

    NodeContainer switchNode;
    switchNode.Create(1);

    NodeContainer router;
    router.Create(1);

    // Create links between hosts and L3 switch
    PointToPointHelper p2pSwitchHost;
    p2pSwitchHost.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pSwitchHost.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer hostDevices[3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 2; ++j) {
            NetDeviceContainer dev = p2pSwitchHost.Install(hosts.Get(i*2 + j), switchNode.Get(0));
            hostDevices[i].Add(dev.Get(0)); // Host side device
        }
    }

    // Create link between L3 switch and router
    PointToPointHelper p2pSwitchRouter;
    p2pSwitchRouter.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    p2pSwitchRouter.SetChannelAttribute("Delay", StringValue("2ms"));
    NetDeviceContainer switchRouterDevices = p2pSwitchRouter.Install(switchNode.Get(0), router.Get(0));

    // Install internet stack
    InternetStackHelper stack;
    stack.InstallAll();

    // Assign VLAN tags to the switch node interfaces connected to hosts
    VlanHelper vlanHelper;
    for (int i = 0; i < 3; ++i) {
        uint16_t vlanId = static_cast<uint16_t>(i + 10);  // VLAN IDs: 10, 20, 30
        for (uint32_t j = 0; j < hostDevices[i].GetN(); ++j) {
            vlanHelper.SetVlanId(hostDevices[i].Get(j), vlanId);
        }
    }

    // Assign IP addresses using DHCP
    DhcpServerHelper dhcpServer(router->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), "255.255.255.0");
    ApplicationContainer dhcpApps = dhcpServer.Install(router);
    dhcpApps.Start(Seconds(1.0));
    dhcpApps.Stop(Seconds(20.0));

    for (auto& host : hosts.Get()) {
        Ptr<DhcpClient> client = CreateObject<DhcpClient>();
        host->AddApplication(client);
        client->SetStartTime(Seconds(2.0));
        client->SetStopTime(Seconds(20.0));
    }

    // Manually configure router interface and subnets
    Ipv4AddressHelper ipv4RouterSwitch;
    ipv4RouterSwitch.SetBase("192.168.100.0", "255.255.255.0");
    Ipv4InterfaceContainer routerSwitchIfaces = ipv4RouterSwitch.Assign(switchRouterDevices.Get(1));

    // Router will have multiple IPs for each VLAN subnet
    Ptr<Ipv4> routerIpv4 = router->GetObject<Ipv4>();
    uint32_t routerInterfaceIndex = routerSwitchIfaces.GetInterfaceIndex();
    routerIpv4->AddAddress(routerInterfaceIndex, Ipv4InterfaceAddress(Ipv4Address("192.168.10.1"), Ipv4Mask("255.255.255.0")));
    routerIpv4->AddAddress(routerInterfaceIndex, Ipv4InterfaceAddress(Ipv4Address("192.168.20.1"), Ipv4Mask("255.255.255.0")));
    routerIpv4->AddAddress(routerInterfaceIndex, Ipv4InterfaceAddress(Ipv4Address("192.168.30.1"), Ipv4Mask("255.255.255.0")));

    // Set default route on hosts via router
    Ipv4StaticRoutingHelper routingHelper;
    for (auto& host : hosts.Get()) {
        Ptr<Ipv4StaticRouting> hostRouting = routingHelper.GetStaticRouting(host->GetObject<Ipv4>());
        hostRouting->SetDefaultRoute(Ipv4Address("192.168.100.1"), routerInterfaceIndex);
    }

    // Enable global routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup traffic between hosts in different VLANs
    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(hosts.Get(0));
    serverApps.Start(Seconds(5.0));
    serverApps.Stop(Seconds(20.0));

    UdpEchoClientHelper echoClient(hosts.Get(0)->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal(), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(hosts.Get(5));
    clientApps.Start(Seconds(6.0));
    clientApps.Stop(Seconds(20.0));

    // Pcap tracing for DHCP packets
    p2pSwitchHost.EnablePcapAll("vlan_dhcp_inter_routing");

    Simulator::Stop(Seconds(20.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}