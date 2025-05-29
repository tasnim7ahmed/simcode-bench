#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/dhcp-server-module.h"
#include "ns3/dhcp-client-module.h"
#include "ns3/vlan-tag.h"
#include "ns3/net-device-container.h"
#include "ns3/tap-bridge-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    Time::SetResolution(Time::NS);

    NodeContainer vlanswitch;
    vlanswitch.Create(1);

    NodeContainer router;
    router.Create(1);

    NodeContainer vlan1Nodes;
    vlan1Nodes.Create(2);
    NodeContainer vlan2Nodes;
    vlan2Nodes.Create(2);
    NodeContainer vlan3Nodes;
    vlan3Nodes.Create(2);

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    NetDeviceContainer vlan1SwitchRouterDevices = p2p.Install(vlanswitch.Get(0), router.Get(0));
    NetDeviceContainer vlan2SwitchRouterDevices = p2p.Install(vlanswitch.Get(0), router.Get(0));
    NetDeviceContainer vlan3SwitchRouterDevices = p2p.Install(vlanswitch.Get(0), router.Get(0));

    NetDeviceContainer vlan1SwitchDevices = p2p.Install(vlanswitch.Get(0), vlan1Nodes);
    NetDeviceContainer vlan2SwitchDevices = p2p.Install(vlanswitch.Get(0), vlan2Nodes);
    NetDeviceContainer vlan3SwitchDevices = p2p.Install(vlanswitch.Get(0), vlan3Nodes);

    VlanTag tag1 = VlanTag(10);
    VlanTag tag2 = VlanTag(20);
    VlanTag tag3 = VlanTag(30);

    NetDeviceContainer vlan1TaggedSwitchDevices;
    vlan1TaggedSwitchDevices.Add(vlan1SwitchRouterDevices.Get(0));
    vlan1TaggedSwitchDevices.Add(vlan2SwitchRouterDevices.Get(0));
    vlan1TaggedSwitchDevices.Add(vlan3SwitchRouterDevices.Get(0));

    NetDeviceContainer vlan1TaggedDevices;
    for (uint32_t i = 0; i < vlan1SwitchDevices.GetN(); ++i) {
      vlan1TaggedDevices.Add(vlan1SwitchDevices.Get(i));
    }
    NetDeviceContainer vlan2TaggedDevices;
    for (uint32_t i = 0; i < vlan2SwitchDevices.GetN(); ++i) {
      vlan2TaggedDevices.Add(vlan2SwitchDevices.Get(i));
    }
    NetDeviceContainer vlan3TaggedDevices;
    for (uint32_t i = 0; i < vlan3SwitchDevices.GetN(); ++i) {
      vlan3TaggedDevices.Add(vlan3SwitchDevices.Get(i));
    }

    for (uint32_t i = 0; i < vlan1TaggedDevices.GetN(); ++i) {
      Ptr<VlanNetDevice> vlanDevice = CreateObject<VlanNetDevice>();
      vlanDevice->SetNetDevice(vlan1TaggedDevices.Get(i));
      vlanDevice->SetVlanId(tag1.GetVlanId());
      vlan1TaggedDevices.Replace(i, vlanDevice);
    }
    for (uint32_t i = 0; i < vlan2TaggedDevices.GetN(); ++i) {
      Ptr<VlanNetDevice> vlanDevice = CreateObject<VlanNetDevice>();
      vlanDevice->SetNetDevice(vlan2TaggedDevices.Get(i));
      vlanDevice->SetVlanId(tag2.GetVlanId());
      vlan2TaggedDevices.Replace(i, vlanDevice);
    }
    for (uint32_t i = 0; i < vlan3TaggedDevices.GetN(); ++i) {
      Ptr<VlanNetDevice> vlanDevice = CreateObject<VlanNetDevice>();
      vlanDevice->SetNetDevice(vlan3TaggedDevices.Get(i));
      vlanDevice->SetVlanId(tag3.GetVlanId());
      vlan3TaggedDevices.Replace(i, vlanDevice);
    }

    InternetStackHelper stack;
    stack.Install(vlan1Nodes);
    stack.Install(vlan2Nodes);
    stack.Install(vlan3Nodes);
    stack.Install(router);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan1Interfaces = address.Assign(vlan1TaggedDevices);
    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan2Interfaces = address.Assign(vlan2TaggedDevices);
    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer vlan3Interfaces = address.Assign(vlan3TaggedDevices);

    address.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer routerInterfacesVlan1 = address.Assign(vlan1SwitchRouterDevices.Get(1));
    address.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer routerInterfacesVlan2 = address.Assign(vlan2SwitchRouterDevices.Get(1));
    address.SetBase("10.1.6.0", "255.255.255.0");
    Ipv4InterfaceContainer routerInterfacesVlan3 = address.Assign(vlan3SwitchRouterDevices.Get(1));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    DhcpServerHelper dhcpServerHelper;
    dhcpServerHelper.EnableTraces();

    Ptr<Node> dhcpServerNodeVlan1 = router.Get(0);
    Ptr<Node> dhcpServerNodeVlan2 = router.Get(0);
    Ptr<Node> dhcpServerNodeVlan3 = router.Get(0);

    DhcpServer dhcpServerVlan1;
    dhcpServerVlan1.leaseTime = Seconds(600);
    dhcpServerVlan1.startAddress = Ipv4Address("10.1.1.10");
    dhcpServerVlan1.endAddress = Ipv4Address("10.1.1.20");

    DhcpServer dhcpServerVlan2;
    dhcpServerVlan2.leaseTime = Seconds(600);
    dhcpServerVlan2.startAddress = Ipv4Address("10.1.2.10");
    dhcpServerVlan2.endAddress = Ipv4Address("10.1.2.20");

    DhcpServer dhcpServerVlan3;
    dhcpServerVlan3.leaseTime = Seconds(600);
    dhcpServerVlan3.startAddress = Ipv4Address("10.1.3.10");
    dhcpServerVlan3.endAddress = Ipv4Address("10.1.3.20");

    dhcpServerHelper.Install(routerInterfacesVlan1.GetAddress(0), routerInterfacesVlan1.GetAddress(0), dhcpServerNodeVlan1, dhcpServerVlan1);
    dhcpServerHelper.Install(routerInterfacesVlan2.GetAddress(0), routerInterfacesVlan2.GetAddress(0), dhcpServerNodeVlan2, dhcpServerVlan2);
    dhcpServerHelper.Install(routerInterfacesVlan3.GetAddress(0), routerInterfacesVlan3.GetAddress(0), dhcpServerNodeVlan3, dhcpServerVlan3);

    DhcpClientHelper dhcpClientHelper;
    dhcpClientHelper.Install(vlan1Nodes);
    dhcpClientHelper.Install(vlan2Nodes);
    dhcpClientHelper.Install(vlan3Nodes);

    for (uint32_t i = 0; i < vlan1SwitchDevices.GetN(); ++i){
        p2p.EnablePcap("dhcp-vlan1", vlan1SwitchDevices.Get(i), true);
    }
    for (uint32_t i = 0; i < vlan2SwitchDevices.GetN(); ++i){
        p2p.EnablePcap("dhcp-vlan2", vlan2SwitchDevices.Get(i), true);
    }
    for (uint32_t i = 0; i < vlan3SwitchDevices.GetN(); ++i){
        p2p.EnablePcap("dhcp-vlan3", vlan3SwitchDevices.Get(i), true);
    }

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(vlan1Nodes.Get(0));
    serverApps.Start(Seconds(2.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(vlan1Interfaces.GetAddress(0), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(10));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));
    ApplicationContainer clientApps = echoClient.Install(vlan2Nodes.Get(0));
    clientApps.Start(Seconds(3.0));
    clientApps.Stop(Seconds(10.0));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}