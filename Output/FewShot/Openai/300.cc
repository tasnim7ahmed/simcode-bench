#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/csma-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/zigbee-module.h"

using namespace ns3;

int main(int argc, char *argv[]) {
    uint32_t nNodes = 10;
    double simTime = 10.0; // seconds

    // Enable logging if needed
    // LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    // LogComponentEnable("UdpServer", LOG_LEVEL_INFO);

    NodeContainer nodes;
    nodes.Create(nNodes);

    // ZigBee device and channel setup
    ZigbeeHelper zigbee;
    zigbee.SetChannel("ns3::ZigbeeChannel");
    zigbee.SetStandard(ZigbeeHelper::ZIGBEE_PRO);

    // Coordinator: node 0 (PAN coordinator)
    // End devices: nodes 1..9
    NetDeviceContainer devices;
    devices = zigbee.Install(nodes);

    // Assign ZigBee roles
    for (uint32_t i = 0; i < nNodes; ++i) {
        Ptr<ZigbeeNetDevice> zd = DynamicCast<ZigbeeNetDevice>(devices.Get(i));
        if (i == 0) {
            zd->SetMacRole(ZIGBEE_MAC_ROLE_COORDINATOR);
        } else {
            zd->SetMacRole(ZIGBEE_MAC_ROLE_END_DEVICE);
        }
    }

    // Set up the Internet stack with IPv6 (ZigBee runs on 802.15.4/IPv6/UDP)
    InternetStackHelper internetv6;
    internetv6.Install(nodes);

    // Assign PAN ID
    uint16_t panId = 0x1234;
    zigbee.SetPanId(devices, panId);

    // Assign IEEE 802.15.4 addresses (use IPv6)
    Ipv6AddressHelper ipv6;
    ipv6.SetBase(Ipv6Address("2001:1::"), Ipv6Prefix(64));
    Ipv6InterfaceContainer ifs = ipv6.Assign(devices);

    // RandomWalk2dMobilityModel within 100x100m area
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
        "Bounds", RectangleValue(Rectangle(0.0, 100.0, 0.0, 100.0)),
        "Distance", DoubleValue(2.0),
        "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
        "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
        "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));
    mobility.Install(nodes);

    // Enable PCAP tracing for ZigBee devices
    zigbee.EnablePcapAll("wsn-zigbee");

    // UDP applications setup
    uint16_t serverPort = 4000;
    // Coordinator server (on node 0)
    UdpServerHelper udpServer(serverPort);
    ApplicationContainer serverApps = udpServer.Install(nodes.Get(0));
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simTime));

    // Each end device (nodes 1..9) sends a 128-byte UDP packet to coordinator every 500ms
    for (uint32_t i = 1; i < nNodes; ++i) {
        UdpClientHelper udpClient(ifs.GetAddress(0, 1), serverPort);
        udpClient.SetAttribute("MaxPackets", UintegerValue(10000)); // Much higher than simulation length would reach
        udpClient.SetAttribute("Interval", TimeValue(MilliSeconds(500)));
        udpClient.SetAttribute("PacketSize", UintegerValue(128));
        ApplicationContainer clientApps = udpClient.Install(nodes.Get(i));
        clientApps.Start(Seconds(1.0));
        clientApps.Stop(Seconds(simTime));
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}