#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiAggregationTest");

void
TraceTxopDuration(uint16_t staId, double distance, Time duration)
{
    std::cout << "STA ID: " << staId << ", Distance: " << distance << "m, TXOP Duration: " << duration.As(Time::MS) << std::endl;
}

int
main(int argc, char* argv[])
{
    uint32_t payloadSize = 1472; // bytes
    double simulationTime = 10;  // seconds
    bool enableRtsCts = false;
    double distance = 1.0;       // meters
    uint32_t txopLimitUsA = 0;   // microseconds (0 means default)
    uint32_t txopLimitUsB = 0;
    uint32_t txopLimitUsC = 0;
    uint32_t txopLimitUsD = 0;

    CommandLine cmd(__FILE__);
    cmd.AddValue("enableRtsCts", "Enable RTS/CTS for all stations", enableRtsCts);
    cmd.AddValue("distance", "Distance between AP and STA (m)", distance);
    cmd.AddValue("txopLimitUsA", "TXOP limit in microseconds for Station A", txopLimitUsA);
    cmd.AddValue("txopLimitUsB", "TXOP limit in microseconds for Station B", txopLimitUsB);
    cmd.AddValue("txopLimitUsC", "TXOP limit in microseconds for Station C", txopLimitUsC);
    cmd.AddValue("txopLimitUsD", "TXOP limit in microseconds for Station D", txopLimitUsD);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue(enableRtsCts ? "0" : "2347"));

    NodeContainer apNodes;
    NodeContainer staNodes;
    apNodes.Create(4);
    staNodes.Create(4);

    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper = YansWifiPhyHelper::Default();
    phyHelper.SetChannel(channelHelper.Create());

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n_5GHZ);

    NetDeviceContainer apDevices[4];
    NetDeviceContainer staDevices[4];

    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNodes);

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(distance),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(10.0),
                                  "DeltaY", DoubleValue(0.0),
                                  "GridWidth", UintegerValue(4),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.Install(staNodes);

    // Network A: Default A-MPDU with max 65kB
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));
    WifiMacHelper macHelper;
    Ssid ssidA = Ssid("network-A");
    macHelper.SetType("ns3::ApWifiMac",
                      "Ssid", SsidValue(ssidA),
                      "BeaconInterval", TimeValue(MicroSeconds(102400)));
    phyHelper.Set("ChannelNumber", UintegerValue(36));
    apDevices[0] = wifi.Install(phyHelper, macHelper, apNodes.Get(0));
    macHelper.SetType("ns3::StaWifiMac",
                      "Ssid", SsidValue(ssidA),
                      "ActiveProbing", BooleanValue(false));
    staDevices[0] = wifi.Install(phyHelper, macHelper, staNodes.Get(0));
    Config::Set("/NodeList/0/DeviceList/0/Mac/BE_Txop/Ampdu", BooleanValue(true));
    Config::Set("/NodeList/0/DeviceList/0/Mac/BE_Txop/MaxAmpduSize", UintegerValue(65535));
    if (txopLimitUsA > 0)
    {
        Config::Set("/NodeList/0/DeviceList/0/Mac/BE_Txop/TxopLimit", TimeValue(MicroSeconds(txopLimitUsA)));
    }

    // Network B: Aggregation disabled
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));
    WifiMacHelper macHelperB;
    Ssid ssidB = Ssid("network-B");
    macHelperB.SetType("ns3::ApWifiMac",
                       "Ssid", SsidValue(ssidB),
                       "BeaconInterval", TimeValue(MicroSeconds(102400)));
    phyHelper.Set("ChannelNumber", UintegerValue(40));
    apDevices[1] = wifi.Install(phyHelper, macHelperB, apNodes.Get(1));
    macHelperB.SetType("ns3::StaWifiMac",
                       "Ssid", SsidValue(ssidB),
                       "ActiveProbing", BooleanValue(false));
    staDevices[1] = wifi.Install(phyHelper, macHelperB, staNodes.Get(1));
    Config::Set("/NodeList/1/DeviceList/0/Mac/BE_Txop/Ampdu", BooleanValue(false));
    Config::Set("/NodeList/1/DeviceList/0/Mac/BE_Txop/Amsdu", BooleanValue(false));
    if (txopLimitUsB > 0)
    {
        Config::Set("/NodeList/1/DeviceList/0/Mac/BE_Txop/TxopLimit", TimeValue(MicroSeconds(txopLimitUsB)));
    }

    // Network C: A-MSDU only, max size 8KB, disable A-MPDU
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));
    WifiMacHelper macHelperC;
    Ssid ssidC = Ssid("network-C");
    macHelperC.SetType("ns3::ApWifiMac",
                       "Ssid", SsidValue(ssidC),
                       "BeaconInterval", TimeValue(MicroSeconds(102400)));
    phyHelper.Set("ChannelNumber", UintegerValue(44));
    apDevices[2] = wifi.Install(phyHelper, macHelperC, apNodes.Get(2));
    macHelperC.SetType("ns3::StaWifiMac",
                       "Ssid", SsidValue(ssidC),
                       "ActiveProbing", BooleanValue(false));
    staDevices[2] = wifi.Install(phyHelper, macHelperC, staNodes.Get(2));
    Config::Set("/NodeList/2/DeviceList/0/Mac/BE_Txop/Ampdu", BooleanValue(false));
    Config::Set("/NodeList/2/DeviceList/0/Mac/BE_Txop/Amsdu", BooleanValue(true));
    Config::Set("/NodeList/2/DeviceList/0/Mac/BE_Txop/MaxAmsduSize", UintegerValue(8192));
    if (txopLimitUsC > 0)
    {
        Config::Set("/NodeList/2/DeviceList/0/Mac/BE_Txop/TxopLimit", TimeValue(MicroSeconds(txopLimitUsC)));
    }

    // Network D: A-MPDU + A-MSDU
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("HtMcs7"), "ControlMode", StringValue("HtMcs0"));
    WifiMacHelper macHelperD;
    Ssid ssidD = Ssid("network-D");
    macHelperD.SetType("ns3::ApWifiMac",
                       "Ssid", SsidValue(ssidD),
                       "BeaconInterval", TimeValue(MicroSeconds(102400)));
    phyHelper.Set("ChannelNumber", UintegerValue(48));
    apDevices[3] = wifi.Install(phyHelper, macHelperD, apNodes.Get(3));
    macHelperD.SetType("ns3::StaWifiMac",
                       "Ssid", SsidValue(ssidD),
                       "ActiveProbing", BooleanValue(false));
    staDevices[3] = wifi.Install(phyHelper, macHelperD, staNodes.Get(3));
    Config::Set("/NodeList/3/DeviceList/0/Mac/BE_Txop/Ampdu", BooleanValue(true));
    Config::Set("/NodeList/3/DeviceList/0/Mac/BE_Txop/MaxAmpduSize", UintegerValue(32768));
    Config::Set("/NodeList/3/DeviceList/0/Mac/BE_Txop/Amsdu", BooleanValue(true));
    Config::Set("/NodeList/3/DeviceList/0/Mac/BE_Txop/MaxAmsduSize", UintegerValue(4096));
    if (txopLimitUsD > 0)
    {
        Config::Set("/NodeList/3/DeviceList/0/Mac/BE_Txop/TxopLimit", TimeValue(MicroSeconds(txopLimitUsD)));
    }

    InternetStackHelper stack;
    stack.Install(apNodes);
    stack.Install(staNodes);

    Ipv4AddressHelper address;
    Ipv4InterfaceContainer apInterfaces[4], staInterfaces[4];
    for (int i = 0; i < 4; ++i)
    {
        std::ostringstream subnet;
        subnet << "10.0." << i << ".0";
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        apInterfaces[i] = address.Assign(apDevices[i]);
        staInterfaces[i] = address.Assign(staDevices[i]);
    }

    UdpServerHelper serverHelper;
    ApplicationContainer serverApps;
    for (int i = 0; i < 4; ++i)
    {
        serverApps.Add(serverHelper.Install(staNodes.Get(i)));
    }
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(Seconds(simulationTime));

    UdpClientHelper clientHelper;
    clientHelper.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(0.0001)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(payloadSize));
    ApplicationContainer clientApps;
    for (int i = 0; i < 4; ++i)
    {
        clientHelper.SetAttribute("RemoteAddress", AddressValue(staInterfaces[i].GetAddress(0)));
        clientApps.Add(clientHelper.Install(apNodes.Get(i)));
    }
    clientApps.Start(Seconds(1.0));
    clientApps.Stop(Seconds(simulationTime));

    Config::Connect("/NodeList/0/DeviceList/0/Mac/BE_Txop/TxopUsed", MakeBoundCallback(&TraceTxopDuration, 0, distance));
    Config::Connect("/NodeList/1/DeviceList/0/Mac/BE_Txop/TxopUsed", MakeBoundCallback(&TraceTxopDuration, 1, distance));
    Config::Connect("/NodeList/2/DeviceList/0/Mac/BE_Txop/TxopUsed", MakeBoundCallback(&TraceTxopDuration, 2, distance));
    Config::Connect("/NodeList/3/DeviceList/0/Mac/BE_Txop/TxopUsed", MakeBoundCallback(&TraceTxopDuration, 3, distance));

    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();

    for (int i = 0; i < 4; ++i)
    {
        uint32_t totalRxBytes = DynamicCast<UdpServer>(serverApps.Get(i))->GetReceived());
        double throughput = (totalRxBytes * 8) / (simulationTime * 1000000.0);
        std::cout << "Network " << static_cast<char>('A' + i) << ": Throughput = " << throughput << " Mbps" << std::endl;
    }

    Simulator::Destroy();
    return 0;
}