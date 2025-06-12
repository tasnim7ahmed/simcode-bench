#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAggregationSimulation");

int main(int argc, char *argv[]) {
    uint32_t payloadSize = 1472; // bytes
    double simulationTime = 10.0; // seconds
    double distance = 1.0; // meters
    bool enableRtsCts = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("distance", "Distance between stations and APs in meters", distance);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS", enableRtsCts);
    cmd.Parse(argc, argv);

    if (enableRtsCts) {
        Config::SetDefault("ns3::WifiMac::RtsThreshold", UintegerValue(1));
    }

    NodeContainer apNodes;
    NodeContainer staNodes;
    apNodes.Create(4);
    staNodes.Create(4);

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211n);

    WifiMacHelper macHelper;
    Ssid ssid = Ssid("wifi-aggregation");

    NetDeviceContainer apDevices;
    NetDeviceContainer staDevices[4];

    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream oss;
        oss << 36 + i * 4;
        phyHelper.Set("ChannelNumber", UintegerValue(std::stoul(oss.str())));
        phyHelper.Set("TxPowerStart", DoubleValue(16.0206));
        phyHelper.Set("TxPowerEnd", DoubleValue(16.0206));
        phyHelper.Set("TxPowerLevels", UintegerValue(1));

        macHelper.SetType("ns3::ApWifiMac",
                          "Ssid", SsidValue(ssid),
                          "BeaconInterval", TimeValue(Seconds(2.5)));

        apDevices.Add(wifiHelper.Install(phyHelper, macHelper, apNodes.Get(i)));

        macHelper.SetType("ns3::StaWifiMac",
                          "Ssid", SsidValue(ssid),
                          "ActiveProbing", BooleanValue(false));

        staDevices[i].Add(wifiHelper.Install(phyHelper, macHelper, staNodes.Get(i)));
    }

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(distance),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);
    mobility.Install(staNodes);

    InternetStackHelper stack;
    stack.InstallAll();

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer apInterfaces[4];
    Ipv4InterfaceContainer staInterfaces[4];

    for (uint32_t i = 0; i < 4; ++i) {
        std::ostringstream network;
        network << "192.168." << i << ".0";
        address.SetBase(network.str().c_str(), "255.255.255.0");
        apInterfaces[i] = address.Assign(apDevices.Get(i));
        staInterfaces[i] = address.Assign(staDevices[i]);
    }

    // Configure aggregation settings
    for (uint32_t i = 0; i < 4; ++i) {
        Ptr<NetDevice> dev = staDevices[i].Get(0);
        Ptr<WifiNetDevice> wifiDev = DynamicCast<WifiNetDevice>(dev);
        Ptr<WifiMac> mac = wifiDev->GetMac();

        if (i == 0) { // Station A: default (A-MSDU off, A-MPDU on at 65KB)
            mac->SetAttribute("BE_MaxAmpduSize", UintegerValue(65535));
            mac->SetAttribute("BK_MaxAmpduSize", UintegerValue(65535));
            mac->SetAttribute("VI_MaxAmpduSize", UintegerValue(65535));
            mac->SetAttribute("VO_MaxAmpduSize", UintegerValue(65535));
            mac->SetAttribute("BE_MaxAmsduSize", UintegerValue(0));
            mac->SetAttribute("BK_MaxAmsduSize", UintegerValue(0));
            mac->SetAttribute("VI_MaxAmsduSize", UintegerValue(0));
            mac->SetAttribute("VO_MaxAmsduSize", UintegerValue(0));
        } else if (i == 1) { // Station B: both disabled
            mac->SetAttribute("BE_MaxAmpduSize", UintegerValue(0));
            mac->SetAttribute("BK_MaxAmpduSize", UintegerValue(0));
            mac->SetAttribute("VI_MaxAmpduSize", UintegerValue(0));
            mac->SetAttribute("VO_MaxAmpduSize", UintegerValue(0));
            mac->SetAttribute("BE_MaxAmsduSize", UintegerValue(0));
            mac->SetAttribute("BK_MaxAmsduSize", UintegerValue(0));
            mac->SetAttribute("VI_MaxAmsduSize", UintegerValue(0));
            mac->SetAttribute("VO_MaxAmsduSize", UintegerValue(0));
        } else if (i == 2) { // Station C: A-MSDU enabled at 8KB, A-MPDU disabled
            mac->SetAttribute("BE_MaxAmpduSize", UintegerValue(0));
            mac->SetAttribute("BK_MaxAmpduSize", UintegerValue(0));
            mac->SetAttribute("VI_MaxAmpduSize", UintegerValue(0));
            mac->SetAttribute("VO_MaxAmpduSize", UintegerValue(0));
            mac->SetAttribute("BE_MaxAmsduSize", UintegerValue(8192));
            mac->SetAttribute("BK_MaxAmsduSize", UintegerValue(8192));
            mac->SetAttribute("VI_MaxAmsduSize", UintegerValue(8192));
            mac->SetAttribute("VO_MaxAmsduSize", UintegerValue(8192));
        } else if (i == 3) { // Station D: two-level aggregation (A-MPDU 32KB, A-MSDU 4KB)
            mac->SetAttribute("BE_MaxAmpduSize", UintegerValue(32768));
            mac->SetAttribute("BK_MaxAmpduSize", UintegerValue(32768));
            mac->SetAttribute("VI_MaxAmpduSize", UintegerValue(32768));
            mac->SetAttribute("VO_MaxAmpduSize", UintegerValue(32768));
            mac->SetAttribute("BE_MaxAmsduSize", UintegerValue(4096));
            mac->SetAttribute("BK_MaxAmsduSize", UintegerValue(4096));
            mac->SetAttribute("VI_MaxAmsduSize", UintegerValue(4096));
            mac->SetAttribute("VO_MaxAmsduSize", UintegerValue(4096));
        }
    }

    uint16_t port = 9;
    ApplicationContainer serverApps;
    ApplicationContainer clientApps;

    for (uint32_t i = 0; i < 4; ++i) {
        UdpEchoServerHelper echoServer(port);
        serverApps.Add(echoServer.Install(apNodes.Get(i)));

        UdpEchoClientHelper echoClient(apInterfaces[i].GetAddress(0), port);
        echoClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
        echoClient.SetAttribute("Interval", TimeValue(Seconds(0.001)));
        echoClient.SetAttribute("PacketSize", UintegerValue(payloadSize));
        clientApps.Add(echoClient.Install(staNodes.Get(i)));
    }

    serverApps.Start(Seconds(0.0));
    clientApps.Start(Seconds(0.0));
    clientApps.Stop(Seconds(simulationTime));
    serverApps.Stop(Seconds(simulationTime));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}