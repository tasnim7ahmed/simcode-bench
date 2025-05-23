#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/dot15d4-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/ipv6-static-routing-helper.h" // Not strictly needed for direct communication but good for IPv6 setup clarity
#include "ns3/netanim-module.h" // Optional, for visualization

using namespace ns3;

int main(int argc, char *argv[])
{
    // Enable verbose logging for relevant components
    LogComponentEnable("Ping6Application", LOG_LEVEL_INFO);
    LogComponentEnable("Dot15d4Mac", LOG_LEVEL_INFO);
    LogComponentEnable("Dot15d4NetDevice", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6L3Protocol", LOG_LEVEL_INFO);
    LogComponentEnable("Ipv6EndPoint", LOG_LEVEL_INFO);
    LogComponentEnable("DropTailQueue", LOG_LEVEL_INFO);

    // Set default queue type to DropTailQueue for Dot15d4 MAC's transmit and receive queues
    // This must be set before installing devices.
    Config::SetDefault("ns3::Dot15d4Mac::TxQueueType", StringValue("ns3::DropTailQueue"));
    Config::SetDefault("ns3::Dot15d4Mac::RxQueueType", StringValue("ns3::DropTailQueue"));

    // Create two nodes for the Wireless Sensor Network
    NodeContainer nodes;
    nodes.Create(2);

    // Set up stationary mobility for both nodes
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Assign specific positions to the nodes
    Ptr<ConstantPositionMobilityModel> pos0 = nodes.Get(0)->GetObject<ConstantPositionMobilityModel>();
    pos0->SetPosition(Vector(0.0, 0.0, 0.0)); // Node 0 at origin

    Ptr<ConstantPositionMobilityModel> pos1 = nodes.Get(1)->GetObject<ConstantPositionMobilityModel>();
    pos1->SetPosition(Vector(1.0, 1.0, 0.0)); // Node 1 at (1,1,0), within typical 802.15.4 range

    // Create and install IEEE 802.15.4 devices on the nodes
    Dot15d4Helper dot15d4Helper;
    NetDeviceContainer devices = dot15d4Helper.Install(nodes);

    // Configure Dot15d4 MAC attributes: coordinator role and short addresses
    Ptr<Dot15d4Mac> mac0 = DynamicCast<Dot15d4Mac>(devices.Get(0)->GetObject<Dot15d4NetDevice>()->GetMac());
    mac0->SetCoordinator(true);         // Node 0 is the coordinator
    mac0->SetAssociationAllowed(true);  // Allow associations (though not explicitly modelled for 2 nodes)
    mac0->SetShortAddress(0x0001);      // Assign short address for coordinator

    Ptr<Dot15d4Mac> mac1 = DynamicCast<Dot15d4Mac>(devices.Get(1)->GetObject<Dot15d4NetDevice>()->GetMac());
    mac1->SetCoordinator(false);        // Node 1 is an end device
    mac1->SetShortAddress(0x0002);      // Assign short address for end device

    // Install the IPv6 Internet stack on both nodes
    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    // Assign IPv6 addresses to the Dot15d4 interfaces
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64)); // Use a common prefix
    Ipv6InterfaceContainer interfaces = ipv6.Assign(devices);

    // Set up Ping6 application from Node 0 (sender) to Node 1 (receiver)
    Ptr<Node> senderNode = nodes.Get(0);
    Ptr<Node> receiverNode = nodes.Get(1);

    // Get the IPv6 address of Node 1's interface (which will be the target for ping)
    Ipv6Address receiverAddress = interfaces.GetAddress(1, 0); // Interface 1 (Node 1's device), 0th address

    Ping6Helper ping6;
    ping6.SetTargetAddress(receiverAddress);
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0))); // Send a ping every 1 second
    ping6.SetAttribute("PacketSize", UintegerValue(100));     // Ping packet size of 100 bytes
    ping6.SetAttribute("MaxPackets", UintegerValue(5));       // Send 5 ping packets

    ApplicationContainer pingApps = ping6.Install(senderNode);
    pingApps.Start(Seconds(1.0));  // Start sending pings at 1 second into simulation
    pingApps.Stop(Seconds(10.0)); // Stop the application at 10 seconds

    // Configure tracing to 'wsn-ping6.tr' for ASCII trace
    // This will combine all device traces into a single file.
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wsn-ping6.tr");
    dot15d4Helper.EnableAsciiAll(stream, devices);

    // Enable PCAP tracing for packet capture (per device)
    dot15d4Helper.EnablePcapAll("wsn-ping6", devices);

    // Set simulation stop time
    Simulator::Stop(Seconds(15.0)); // Run simulation for 15 seconds

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}