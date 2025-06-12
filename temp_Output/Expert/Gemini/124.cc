#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include <sstream>

using namespace ns3;

int main(int argc, char *argv[]) {
    CommandLine cmd;
    cmd.Parse(argc, argv);

    NodeContainer hubNode;
    hubNode.Create(1);

    NodeContainer subnetNodes;
    subnetNodes.Create(3);

    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", DataRateValue(DataRate("10Mbps")));
    csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6560)));

    NetDeviceContainer hubDevice, subnet1Device, subnet2Device, subnet3Device;

    NodeContainer subnet1Nodes;
    subnet1Nodes.Add(subnetNodes.Get(0));
    subnet1Nodes.Add(hubNode.Get(0));
    NetDeviceContainer devicesSubnet1 = csma.Install(subnet1Nodes);
    subnet1Device = devicesSubnet1.Get(0);
    hubDevice.Add(devicesSubnet1.Get(1));

    NodeContainer subnet2Nodes;
    subnet2Nodes.Add(subnetNodes.Get(1));
    subnet2Nodes.Add(hubNode.Get(0));
    NetDeviceContainer devicesSubnet2 = csma.Install(subnet2Nodes);
    subnet2Device = devicesSubnet2.Get(0);
    hubDevice.Add(devicesSubnet2.Get(1));

    NodeContainer subnet3Nodes;
    subnet3Nodes.Add(subnetNodes.Get(2));
    subnet3Nodes.Add(hubNode.Get(0));
    NetDeviceContainer devicesSubnet3 = csma.Install(subnet3Nodes);
    subnet3Device = devicesSubnet3.Get(0);
    hubDevice.Add(devicesSubnet3.Get(1));

    InternetStackHelper stack;
    stack.Install(hubNode);
    stack.Install(subnetNodes);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer subnet1Interfaces = address.Assign(devicesSubnet1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer subnet2Interfaces = address.Assign(devicesSubnet2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer subnet3Interfaces = address.Assign(devicesSubnet3);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    UdpEchoServerHelper echoServer(port);

    ApplicationContainer serverApps = echoServer.Install(subnetNodes.Get(2));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(subnet1Interfaces.GetAddress(0), port);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(subnetNodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    csma.EnablePcapAll("hub");

    Simulator::Run(Seconds(10.0));
    Simulator::Destroy();
    return 0;
}