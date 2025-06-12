#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
    Time::SetResolution (Time::NS);

    // Create two nodes
    NodeContainer nodes;
    nodes.Create (2);

    // Configure point-to-point link
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

    // Install devices and channels
    NetDeviceContainer devices;
    devices = pointToPoint.Install (nodes);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install (nodes);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign (devices);

    // Setup TCP Server Application on node 1
    uint16_t port = 8080;
    Address localAddress (InetSocketAddress (Ipv4Address::GetAny (), port));
    PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory", localAddress);
    ApplicationContainer serverApp = packetSinkHelper.Install (nodes.Get (1));
    serverApp.Start (Seconds (1.0));
    serverApp.Stop (Seconds (10.0));

    // Setup TCP Client Application on node 0
    Ipv4Address serverAddress = interfaces.GetAddress (1);
    Address remoteAddress (InetSocketAddress (serverAddress, port));
    OnOffHelper clientHelper ("ns3::TcpSocketFactory", remoteAddress);
    clientHelper.SetAttribute ("PacketSize", UintegerValue (1024));
    clientHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper.SetAttribute ("DataRate", StringValue ("819.2kbps")); // 1024*8/0.01 = 819200bps = 819.2kbps

    ApplicationContainer clientApp = clientHelper.Install (nodes.Get (0));
    clientApp.Start (Seconds (2.0));
    clientApp.Stop (Seconds (10.0));

    // Set up the number of packets (stop after sending 1000 packets)
    // We'll use a trick with MaxBytes: maxBytes = 1000 * 1024 = 1024000
    uint32_t maxBytes = 1000 * 1024;
    clientHelper.SetAttribute ("MaxBytes", UintegerValue (maxBytes));
    // To make sure the attribute is set, re-assign it (note: needed since 'clientApp' is already installed)
    clientApp.Get (0)->SetAttribute ("MaxBytes", UintegerValue (maxBytes));

    // Enable output for flow monitor (optional)
    // FlowMonitorHelper flowmon;
    // Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

    Simulator::Stop (Seconds (10.0));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}