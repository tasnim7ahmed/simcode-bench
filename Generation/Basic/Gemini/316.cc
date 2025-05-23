#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ssid.h"

int main() {
    // 1. Configure WiFi Standard
    ns3::WifiHelper wifiHelper;
    wifiHelper.SetStandard(ns3::WIFI_STANDARD_80211n);

    // 2. Configure Phy and Channel
    ns3::YansWifiPhyHelper wifiPhy;
    wifiPhy.SetPcapDataLinkType(ns3::WifiPhyHelper::DLT_IEEE802_11_RADIO);
    ns3::YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    // 3. Create Nodes
    ns3::NodeContainer apNode;
    apNode.Create(1);
    ns3::NodeContainer staNode;
    staNode.Create(1);

    // 4. Install Mobility Models
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNode);
    apNode.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(0.0, 0.0, 0.0));
    staNode.Get(0)->GetObject<ns3::ConstantPositionMobilityModel>()->SetPosition(ns3::Vector(1.0, 0.0, 0.0)); // Place STA near AP

    // 5. Configure MAC and Install WiFi Devices
    ns3::Ssid ssid = ns3::Ssid("my-wifi-network");
    ns3::NqosWifiMacHelper wifiMac = ns3::NqosWifiMacHelper::Default();

    ns3::NetDeviceContainer apDevice;
    wifiMac.SetType("ns3::ApWifiMac", "Ssid", ns3::SsidValue(ssid));
    apDevice = wifiHelper.Install(wifiPhy, wifiMac, apNode);

    ns3::NetDeviceContainer staDevice;
    wifiMac.SetType("ns3::StaWifiMac", "Ssid", ns3::SsidValue(ssid), "ActiveProbing", ns3::BooleanValue(false));
    staDevice = wifiHelper.Install(wifiPhy, wifiMac, staNode);

    // 6. Install Internet Stack
    ns3::InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNode);

    // 7. Assign IP Addresses
    ns3::Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ns3::Ipv4InterfaceContainer apIpInterface = ipv4.Assign(apDevice);
    ns3::Ipv4InterfaceContainer staIpInterface = ipv4.Assign(staDevice);

    // Get AP's IP address for client to connect to
    ns3::Ipv4Address apNodeAddress = apIpInterface.GetAddress(0);
    uint16_t port = 9; // UDP port

    // 8. Configure UDP Server on AP
    ns3::PacketSinkHelper packetSinkHelper("ns3::UdpSocketFactory",
                                           ns3::InetSocketAddress(ns3::Ipv4Address::GetAny(), port));
    ns3::ApplicationContainer serverApps = packetSinkHelper.Install(apNode.Get(0));
    serverApps.Start(ns3::Seconds(1.0));
    serverApps.Stop(ns3::Seconds(10.0));

    // 9. Configure UDP Client on STA
    uint32_t numPackets = 1000;
    uint32_t packetSize = 1024; // bytes
    ns3::Time packetInterval = ns3::Seconds(0.01);

    ns3::UdpClientHelper udpClientHelper(apNodeAddress, port);
    udpClientHelper.SetAttribute("MaxPackets", ns3::UintegerValue(numPackets));
    udpClientHelper.SetAttribute("Interval", ns3::TimeValue(packetInterval));
    udpClientHelper.SetAttribute("PacketSize", ns3::UintegerValue(packetSize));

    ns3::ApplicationContainer clientApps = udpClientHelper.Install(staNode.Get(0));
    clientApps.Start(ns3::Seconds(2.0));
    // Client attempts to send 1000 packets with 0.01s interval,
    // which would take 10 seconds of sending time.
    // Simulation stops at 10 seconds, so client must also stop then.
    // Thus, it will send packets from 2s to 10s.
    clientApps.Stop(ns3::Seconds(10.0)); 

    // 10. Simulation Control
    ns3::Simulator::Stop(ns3::Seconds(10.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    return 0;
}