#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include <fstream>
#include <iostream>

using namespace ns3;

std::ofstream outputFile;

void PacketReceived(std::string context, Ptr<const Packet> packet, Address senderAddress, Address receiverAddress) {
    Ipv4Address senderIpv4 = InetSocketAddress::ConvertFrom(senderAddress).GetIpv4();
    Ipv4Address receiverIpv4 = InetSocketAddress::ConvertFrom(receiverAddress).GetIpv4();
    Time now = Simulator::Now();
    outputFile << senderIpv4 << "," << receiverIpv4 << "," << packet->GetSize() << "," << now.GetSeconds() << std::endl;
}

int main(int argc, char *argv[]) {
    outputFile.open("packet_data.csv");
    outputFile << "Source,Destination,PacketSize,ReceptionTime" << std::endl;

    NodeContainer nodes;
    nodes.Create(5);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    Ssid ssid = Ssid("ns-3-ssid");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices = wifi.Install(phy, mac, nodes.Get(0));
    for (int i = 1; i < 5; ++i) {
      NetDeviceContainer temp = wifi.Install(phy, mac, nodes.Get(i));
      staDevices.Add(temp.Get(0));
    }

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid));

    NetDeviceContainer apDevices = wifi.Install(phy, mac, nodes.Get(4));

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(staDevices);
    Ipv4InterfaceContainer apInterface = address.Assign(apDevices);

    uint16_t port = 9;

    UdpClientHelper client(apInterface.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(10));
    client.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    client.SetAttribute("PacketSize", UintegerValue(1024));

    for (int i = 0; i < 4; ++i) {
      ApplicationContainer clientApps = client.Install(nodes.Get(i));
      clientApps.Start(Seconds(2.0 + i));
      clientApps.Stop(Seconds(10.0));
    }

    PacketSinkHelper sink("ns3::UdpSocketFactory",
                           InetSocketAddress(Ipv4Address::GetAny(), port));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(4));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    Config::Connect("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback(&PacketReceived));

    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();

    outputFile.close();

    return 0;
}