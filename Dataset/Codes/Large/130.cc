#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/nat-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NatExample");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

    // Create nodes
    NodeContainer natRouter, privateHosts, publicServer;
    natRouter.Create(1);         // NAT router
    privateHosts.Create(2);      // Two hosts behind the NAT
    publicServer.Create(1);      // Public server

    // Create point-to-point links
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    // Link between the private network and the NAT router
    NetDeviceContainer privateDevices;
    privateDevices = p2p.Install(NodeContainer(privateHosts, natRouter));

    // Link between the NAT router and the public server
    NetDeviceContainer publicDevices;
    publicDevices = p2p.Install(NodeContainer(natRouter, publicServer));

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(privateHosts);
    internet.Install(natRouter);
    internet.Install(publicServer);

    // Assign IP addresses
    Ipv4AddressHelper privateNetwork;
    privateNetwork.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer privateInterfaces = privateNetwork.Assign(privateDevices.Get(0));

    Ipv4AddressHelper publicNetwork;
    publicNetwork.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer publicInterfaces = publicNetwork.Assign(publicDevices.Get(1));

    // Assign public IP to the NAT router's public interface
    publicNetwork.Assign(publicDevices.Get(0));

    // Configure NAT
    NatHelper natHelper;
    natHelper.Install(natRouter.Get(0), publicDevices.Get(0));

    // Install UDP Echo server on the public server
    UdpEchoServerHelper echoServer(9);  // Listening on port 9
    ApplicationContainer serverApp = echoServer.Install(publicServer.Get(0));
    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(10.0));

    // Install UDP Echo client on the private network hosts
    UdpEchoClientHelper echoClient(publicInterfaces.GetAddress(1), 9);  // Destination: public server
    echoClient.SetAttribute("MaxPackets", UintegerValue(5));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApp1 = echoClient.Install(privateHosts.Get(0));
    clientApp1.Start(Seconds(2.0));
    clientApp1.Stop(Seconds(10.0));

    ApplicationContainer clientApp2 = echoClient.Install(privateHosts.Get(1));
    clientApp2.Start(Seconds(2.0));
    clientApp2.Stop(Seconds(10.0));

    // Enable pcap tracing
    p2p.EnablePcapAll("nat-example");

    // Start simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

