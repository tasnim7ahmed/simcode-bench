#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/on-off-application.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("80211nCompressedBlockAck");

int main(int argc, char *argv[]) {
    uint32_t payloadSize = 1000;
    double simulationTime = 10.0;
    std::string phyMode("HtMcs7");
    bool enablePcap = true;
    uint32_t blockAckThreshold = 8;
    uint32_t inactivityTimeout = 50; // ms

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(2);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    WifiMacHelper mac;
    Ssid ssid = Ssid("80211n-CompressedBA");
    mac.SetType("ns3::StaWifiMac",
                "Ssid", SsidValue(ssid),
                "ActiveProbing", BooleanValue(false));
    NetDeviceContainer staDevices;
    staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac",
                "Ssid", SsidValue(ssid),
                "BeaconInterval", TimeValue(Seconds(2.5)),
                "EnableCompressedBlockAck", BooleanValue(true));
    NetDeviceContainer apDevices;
    apDevices = wifi.Install(phy, mac, apNode);

    for (auto dev : apDevices.GetN() ? apDevices : staDevices) {
        auto wifiNetDev = DynamicCast<WifiNetDevice>(dev);
        if (wifiNetDev) {
            auto macLayer = DynamicCast<RegularWifiMac>(wifiNetDev->GetMac());
            if (macLayer) {
                auto edca = macLayer->GetQosTxopManager();
                if (edca) {
                    edca->SetBlockAckThreshold(blockAckThreshold);
                    edca->SetBlockAckInactivityTimeout(inactivityTimeout);
                }
            }
        }
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apInterfaces;
    apInterfaces = address.Assign(apDevices);
    Ipv4InterfaceContainer staInterfaces;
    staInterfaces = address.Assign(staDevices);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(apInterfaces.GetAddress(0), port));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));

    ApplicationContainer apps;
    apps.Add(onoff.Install(staNodes.Get(0)));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simulationTime));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
    apps = sink.Install(apNode);
    apps.Start(Seconds(0.0));

    Icmpv4L4Protocol::Enable(true);

    if (enablePcap) {
        phy.EnablePcap("ap-wifi-device", apDevices.Get(0));
    }

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}