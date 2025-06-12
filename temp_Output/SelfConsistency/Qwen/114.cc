#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6-list-routing-helper.h"
#include "ns3/netanim-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessSensorNetworkPing6");

int main(int argc, char *argv[]) {
    // Enable logging for this simulation
    LogComponentEnable("WirelessSensorNetworkPing6", LOG_LEVEL_ALL);
    LogComponentEnable("Ipv6Interface", LOG_LEVEL_ALL);
    LogComponentEnable("Icmpv6L4Protocol", LOG_LEVEL_INFO);

    // Create nodes
    NS_LOG_INFO("Creating nodes.");
    NodeContainer nodes;
    nodes.Create(2);

    // Setup LR-WPAN (IEEE 802.15.4) channel and devices
    NS_LOG_INFO("Setting up LR-WPAN devices.");
    LrWpanHelper lrWpanHelper;
    NetDeviceContainer lrWpanDevices = lrWpanHelper.Install(nodes);

    // Mobility: stationary nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(2),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Internet stack with IPv6 support
    NS_LOG_INFO("Installing internet stack with IPv6.");
    InternetStackHelper internetv6;
    internetv6.SetIpv4StackEnabled(false);
    internetv6.SetIpv6StackEnabled(true);
    internetv6.Install(nodes);

    // Assign IPv6 addresses with a specific prefix
    NS_LOG_INFO("Assigning IPv6 addresses.");
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(lrWpanDevices);

    // Set default queue type to DropTail
    Config::SetDefault("ns3::DropTailQueue<Packet>::MaxSize",
                       QueueSizeValue(QueueSize("100p")));

    // Install Ping6 application from node 0 to node 1
    NS_LOG_INFO("Installing Ping6 application.");
    V4PingHelper ping6(interfaces.GetAddress(1, 1)); // Ping the second interface of node 1
    ping6.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    ping6.SetAttribute("Size", UintegerValue(64));
    ApplicationContainer apps = ping6.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Setup PCAP tracing
    NS_LOG_INFO("Enabling PCAP tracing.");
    lrWpanHelper.EnablePcapAll("wsn-ping6", false);

    // Setup ASCII trace file for queue behavior
    NS_LOG_INFO("Enabling ASCII trace file.");
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("wsn-ping6.tr");
    lrWpanHelper.EnableAsciiIpv6AllQueues(stream);

    // Run simulation
    NS_LOG_INFO("Running simulation.");
    Simulator::Run();
    Simulator::Destroy();

    NS_LOG_INFO("Simulation completed.");
    return 0;
}