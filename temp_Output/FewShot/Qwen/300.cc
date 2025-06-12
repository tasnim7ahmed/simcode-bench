#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/wifi-module.h"
#include "ns3/energy-module.h"
#include "ns3/lr-wpan-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    // Set simulation time and packet interval
    double simTime = 10.0;
    Time packetInterval = MilliSeconds(500);
    uint32_t payloadSize = 128;

    // Enable LR-WPAN logging for debugging if needed
    LogComponentEnable("LrWpanMac", LOG_LEVEL_INFO);
    LogComponentEnable("LrWpanPhy", LOG_LEVEL_INFO);
    LogComponentEnable("LrWpanCsmaCa", LOG_LEVEL_INFO);
    LogComponentEnable("LrWpanNetDevice", LOG_LEVEL_INFO);

    // Create nodes: one coordinator (node 0), nine end devices
    NodeContainer nodes;
    nodes.Create(10);

    // Create LR-WPAN channel
    LrWpanHelper lrWpanHelper;
    lrWpanHelper.SetChannelParameter("DataRate", DataRateValue(DataRate("250kbps")));
    lrWpanHelper.SetChannelParameter("Frequency", UintegerValue(2450));
    lrWpanHelper.EnableLogComponents();

    NetDeviceContainer lrwpanDevices = lrWpanHelper.Install(nodes);

    // Assign ZigBee PRO mode MAC parameters
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        Ptr<LrWpanNetDevice> device = DynamicCast<LrWpanNetDevice>(lrwpanDevices.Get(i));
        if (i == 0) {
            device->GetMac()->Set(Mac16AddressAllocator::AllocateShortAddr());
            device->GetMac()->SetCoordExtendedAddr(device->GetMac()->GetExtendedAddress());
            device->GetMac()->SetShortAddress(Mac16Address("00:00"));
            device->GetMac()->SetPanId(0x1234);
            device->GetMac()->SetAssociatedCoordinators(1);
        } else {
            device->GetMac()->Set(Mac16AddressAllocator::AllocateShortAddr());
            device->GetMac()->SetPanId(0x1234);
            device->GetMac()->SetAssociatedCoordinators(0);
        }
    }

    // Mobility model - Random Walk
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                              "Bounds", RectangleValue(Rectangle(0, 100, 0, 100)));
    mobility.Install(nodes);

    // Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Assign IPv6 addresses
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:db8::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer interfaces = ipv6.Assign(lrwpanDevices);

    // Set up UDP echo server on the coordinator node (node 0)
    UdpServerHelper udpServer(80); // Listen on port 80
    ApplicationContainer serverApp = udpServer.Install(nodes.Get(0));
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(simTime));

    // Set up UDP clients on end devices (nodes 1-9)
    UdpClientHelper udpClient(interfaces.GetAddress(0, 1), 80); // Coordinator's address
    udpClient.SetAttribute("MaxPackets", UintegerValue(0)); // Unlimited packets
    udpClient.SetAttribute("Interval", TimeValue(packetInterval));
    udpClient.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < nodes.GetN(); ++i) {
        clientApps = udpClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(1.0 + (i - 1) * 0.1)); // Staggered start times to avoid sync issues
        clientApps.Stop(Seconds(simTime));
    }

    // Enable PCAP tracing
    lrWpanHelper.EnablePcapAll("wsn_simulation");

    // Run simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}