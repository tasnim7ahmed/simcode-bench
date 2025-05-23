#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main()
{
    // Set up default TCP type (e.g., NewReno) and segment size
    GlobalAttributeSet::Set("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    GlobalAttributeSet::Set("ns3::TcpSocket::SegmentSize", UintegerValue(1448)); // Typical MSS for 1500 byte MTU

    // Create two nodes: Computer 1 (index 0) and Computer 2 (index 1)
    NodeContainer nodes;
    nodes.Create(2);

    // Configure Point-to-Point link attributes
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("10ms"));

    // Install Point-to-Point devices on the nodes
    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    // Install the Internet stack on the nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IP Addresses from the 192.168.1.0/24 range
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    // Get the IP address of Computer 2 for the client to send to
    Ipv4Address remoteAddress = interfaces.GetAddress(1); // Computer 2's IP address

    // Define the communication port
    uint16_t port = 9;

    // Install Server Application (PacketSink) on Computer 2
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory",
                                      InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer serverApps = packetSinkHelper.Install(nodes.Get(1)); // Install on Computer 2
    serverApps.Start(Seconds(0.0)); // Server starts immediately
    serverApps.Stop(Seconds(12.0)); // Server runs until end of simulation

    // Install Client Application (OnOffApplication) on Computer 1
    // The client sends 10 packets, each 1024 bytes, at 1-second intervals.
    // This is achieved by setting PacketSize, MaxBytes, and a Rate of 1024Bps.
    // OnTime=1.0 and OffTime=0.0 ensures the client tries to send continuously at the specified rate.
    OnOffHelper onoff("ns3::TcpSocketFactory",
                      InetSocketAddress(remoteAddress, port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(1024)); // Each packet is 1024 bytes
    onoff.SetAttribute("Rate", StringValue("1024Bps")); // Rate to send 1024 bytes/second (i.e., 1 packet/second)
    onoff.SetAttribute("MaxBytes", UintegerValue(10 * 1024)); // Total bytes to send (10 packets * 1024 bytes/packet)

    ApplicationContainer clientApps = onoff.Install(nodes.Get(0)); // Install on Computer 1
    clientApps.Start(Seconds(1.0)); // Client starts sending at 1 second
    clientApps.Stop(Seconds(11.0)); // Client stops after sending all packets

    // Set overall simulation stop time
    Simulator::Stop(Seconds(12.0));

    // Run the simulation
    Simulator::Run();

    // Clean up simulation resources
    Simulator::Destroy();

    return 0;
}