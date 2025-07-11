#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: Apache-2.0
#

if(CONFIG_WIFI_NM_WPA_SUPPLICANT)

zephyr_interface_library_named(hostap)

set(HOSTAP_BASE ${ZEPHYR_HOSTAP_MODULE_DIR})
set(WIFI_NM_WPA_SUPPLICANT_BASE ${HOSTAP_BASE}/wpa_supplicant)
set(HOSTAP_SRC_BASE ${HOSTAP_BASE}/src)
set(WIFI_NM_HOSTAPD_BASE ${HOSTAP_BASE}/hostapd)

set(CMAKE_EXE_LINKER_FLAGS "--specs=nosys.specs -lnosys")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DMISSING_SYSCALL_NAMES")

zephyr_include_directories(
  src/
  ${HOSTAP_BASE}/
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/
  ${HOSTAP_SRC_BASE}/
  ${HOSTAP_SRC_BASE}/common/
  ${HOSTAP_SRC_BASE}/eap_common
  ${HOSTAP_SRC_BASE}/eap_server
  ${HOSTAP_SRC_BASE}/radius
  ${HOSTAP_SRC_BASE}/crypto/
  ${HOSTAP_SRC_BASE}/ap/
  ${HOSTAP_SRC_BASE}/drivers/
  ${HOSTAP_SRC_BASE}/rsn_supp
)

target_include_directories(hostap INTERFACE
  ${HOSTAP_SRC_BASE}/utils/
)

zephyr_library_compile_definitions(
  TLS_DEFAULT_CIPHERS=\""DEFAULT:!EXP:!LOW"\"
  CONFIG_SME
  CONFIG_NO_CONFIG_WRITE
  CONFIG_CTRL_IFACE
  CONFIG_NO_RANDOM_POOL
  CONFIG_SHA256
  CONFIG_SHA384
  CONFIG_SHA512
  CONFIG_CTRL_IFACE_ZEPHYR
  CONFIG_SUITEB192
  CONFIG_SUITEB
)


zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_NONE
  CONFIG_NO_PBKDF2
)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_NO_DEBUG
  CONFIG_NO_STDOUT_DEBUG
)

zephyr_library_compile_definitions_ifndef(CONFIG_WIFI_NM_WPA_SUPPLICANT_ROBUST_AV
  CONFIG_NO_ROBUST_AV
)

zephyr_library_compile_definitions_ifndef(CONFIG_WIFI_NM_WPA_SUPPLICANT_WMM_AC
CONFIG_NO_WMM_AC
)

zephyr_library_compile_definitions_ifndef(CONFIG_WIFI_NM_WPA_SUPPLICANT_RRM
CONFIG_NO_RRM
)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_MBO
  CONFIG_MBO
)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_WNM
  CONFIG_WNM
)

zephyr_library_include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${HOSTAP_BASE}/
  ${HOSTAP_SRC_BASE}/utils
  ${HOSTAP_SRC_BASE}/drivers
  ${HOSTAP_BASE}/src
  ${ZEPHYR_BASE}/include
  ${ZEPHYR_BASE}/include/net
  )

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_NO_DEBUG
  CONFIG_NO_STDOUT_DEBUG
)

zephyr_library_sources(
  ${HOSTAP_SRC_BASE}/common/wpa_common.c
  ${HOSTAP_SRC_BASE}/common/ieee802_11_common.c
  ${HOSTAP_SRC_BASE}/common/hw_features_common.c
  ${HOSTAP_SRC_BASE}/common/wpa_ctrl.c
  ${HOSTAP_SRC_BASE}/common/cli.c

  ${HOSTAP_SRC_BASE}/drivers/driver_common.c
  ${HOSTAP_SRC_BASE}/drivers/drivers.c
  ${HOSTAP_SRC_BASE}/utils/crc32.c
  ${HOSTAP_SRC_BASE}/utils/base64.c
  ${HOSTAP_SRC_BASE}/utils/common.c
  ${HOSTAP_SRC_BASE}/utils/wpabuf.c
  ${HOSTAP_SRC_BASE}/utils/bitfield.c
  ${HOSTAP_SRC_BASE}/utils/eloop.c
  ${HOSTAP_SRC_BASE}/utils/os_zephyr.c
  ${HOSTAP_SRC_BASE}/utils/wpa_debug_zephyr.c
  ${HOSTAP_SRC_BASE}/l2_packet/l2_packet_zephyr.c
  ${HOSTAP_SRC_BASE}/drivers/driver_zephyr.c

  ${WIFI_NM_WPA_SUPPLICANT_BASE}/config.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/notify.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/eap_register.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/config_none.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/bssid_ignore.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/wpas_glue.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/scan.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/ctrl_iface.c

  ${WIFI_NM_WPA_SUPPLICANT_BASE}/bss.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/sme.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/wpa_supplicant.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/events.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/wpa_cli_cmds.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/ctrl_iface_zephyr.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/wpa_cli_zephyr.c

  ${HOSTAP_SRC_BASE}/rsn_supp/pmksa_cache.c
  ${HOSTAP_SRC_BASE}/common/ptksa_cache.c

  # Zephyr specific files (glue code)
  src/supp_main.c
  src/supp_api.c
  src/supp_events.c
)

# Advanced features
zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_RRM
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/op_classes.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/rrm.c
)
zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_WMM_AC
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/wmm_ac.c
)
zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_ROBUST_AV
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/robust_av.c
)
zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_MBO
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/mbo.c
)
zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_WNM
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/wnm_sta.c
)

zephyr_library_sources_ifdef(CONFIG_WPA_CLI
  src/wpa_cli.c
)

zephyr_library_sources_ifdef(CONFIG_IEEE80211R
  ${HOSTAP_SRC_BASE}/rsn_supp/wpa_ft.c
  ${HOSTAP_SRC_BASE}/ap/wpa_auth_ft.c
)

if(CONFIG_WIFI_NM_WPA_SUPPLICANT_AP OR CONFIG_WIFI_NM_HOSTAPD_AP)
zephyr_library_sources(
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/ap.c
  ${HOSTAP_SRC_BASE}/ap/ap_config.c
  ${HOSTAP_SRC_BASE}/ap/ap_drv_ops.c
  ${HOSTAP_SRC_BASE}/ap/ap_list.c
  ${HOSTAP_SRC_BASE}/ap/ap_mlme.c
  ${HOSTAP_SRC_BASE}/ap/authsrv.c
  ${HOSTAP_SRC_BASE}/ap/beacon.c
  ${HOSTAP_SRC_BASE}/ap/bss_load.c
  ${HOSTAP_SRC_BASE}/ap/dfs.c
  ${HOSTAP_SRC_BASE}/ap/drv_callbacks.c
  ${HOSTAP_SRC_BASE}/ap/ctrl_iface_ap.c
  ${HOSTAP_SRC_BASE}/ap/eap_user_db.c
  ${HOSTAP_SRC_BASE}/ap/hostapd.c
  ${HOSTAP_SRC_BASE}/ap/hw_features.c
  ${HOSTAP_SRC_BASE}/ap/ieee802_11_auth.c
  ${HOSTAP_SRC_BASE}/ap/ieee802_11.c
  ${HOSTAP_SRC_BASE}/ap/comeback_token.c
  ${HOSTAP_SRC_BASE}/ap/ieee802_11_ht.c
  ${HOSTAP_SRC_BASE}/ap/ieee802_11_shared.c
  ${HOSTAP_SRC_BASE}/ap/ieee802_1x.c
  ${HOSTAP_SRC_BASE}/ap/neighbor_db.c
  ${HOSTAP_SRC_BASE}/ap/p2p_hostapd.c
  ${HOSTAP_SRC_BASE}/ap/pmksa_cache_auth.c
  ${HOSTAP_SRC_BASE}/ap/preauth_auth.c
  ${HOSTAP_SRC_BASE}/ap/rrm.c
  ${HOSTAP_SRC_BASE}/ap/sta_info.c
  ${HOSTAP_SRC_BASE}/ap/tkip_countermeasures.c
  ${HOSTAP_SRC_BASE}/ap/utils.c
  ${HOSTAP_SRC_BASE}/ap/wmm.c

  ${HOSTAP_SRC_BASE}/ap/wpa_auth.c
  ${HOSTAP_SRC_BASE}/ap/wpa_auth_ie.c
  ${HOSTAP_SRC_BASE}/ap/wpa_auth_ft.c
  ${HOSTAP_SRC_BASE}/ap/wpa_auth_glue.c

  ${HOSTAP_SRC_BASE}/eap_common/eap_common.c
  ${HOSTAP_SRC_BASE}/eap_server/eap_server.c
  ${HOSTAP_SRC_BASE}/eap_server/eap_server_identity.c
  ${HOSTAP_SRC_BASE}/eap_server/eap_server_methods.c
  ${HOSTAP_SRC_BASE}/eapol_auth/eapol_auth_sm.c
  ${HOSTAP_SRC_BASE}/ap/ctrl_iface_ap.c
  ${HOSTAP_SRC_BASE}/utils/crc32.c
  ${HOSTAP_SRC_BASE}/utils/ip_addr.c
)

zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_11AC
  ${HOSTAP_SRC_BASE}/ap/ieee802_11_vht.c
)

if (CONFIG_WIFI_NM_WPA_SUPPLICANT_MBO)
  zephyr_library_sources(${HOSTAP_SRC_BASE}/ap/mbo_ap.c)
endif()

zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_11AX
  ${HOSTAP_SRC_BASE}/ap/ieee802_11_he.c
)

zephyr_library_compile_definitions(
  CONFIG_AP
  CONFIG_NO_RADIUS
  CONFIG_NO_VLAN
  CONFIG_NO_ACCOUNTING
  NEED_AP_MLME
  EAP_SERVER
  EAP_SERVER_IDENTITY
)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_11AC
  CONFIG_IEEE80211AC
)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_11AX
  CONFIG_IEEE80211AX
)
endif()

zephyr_include_directories_ifdef(CONFIG_WIFI_NM_HOSTAPD_AP
  ${WIFI_NM_HOSTAPD_BASE}/
)

zephyr_library_sources_ifdef(CONFIG_WIFI_NM_HOSTAPD_AP
  ${HOSTAP_SRC_BASE}/ap/acs.c
  ${WIFI_NM_HOSTAPD_BASE}/config_file.c
  ${WIFI_NM_HOSTAPD_BASE}/ctrl_iface.c
  ${WIFI_NM_HOSTAPD_BASE}/ctrl_iface_zephyr.c
  ${WIFI_NM_HOSTAPD_BASE}/eap_register.c
  ${WIFI_NM_HOSTAPD_BASE}/hostapd_cli_cmds.c
  ${WIFI_NM_HOSTAPD_BASE}/hostapd_cli_zephyr.c

  # Zephyr specific files (glue code)
  src/hapd_main.c
  src/hapd_api.c
  src/hapd_events.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_HOSTAPD_AP
  HOSTAPD
  CONFIG_ACS
)

zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_NONE
  ${HOSTAP_SRC_BASE}/crypto/crypto_none.c

  # FIXME: why do we need these when crypto is not selected?
  ${HOSTAP_SRC_BASE}/crypto/aes-wrap.c
  ${HOSTAP_SRC_BASE}/crypto/aes-unwrap.c
  ${HOSTAP_SRC_BASE}/crypto/aes-internal.c
  ${HOSTAP_SRC_BASE}/crypto/aes-internal-enc.c
  ${HOSTAP_SRC_BASE}/crypto/aes-internal-dec.c
  ${HOSTAP_SRC_BASE}/crypto/aes-omac1.c
  ${HOSTAP_SRC_BASE}/crypto/md5.c
  ${HOSTAP_SRC_BASE}/crypto/md5-internal.c
  ${HOSTAP_SRC_BASE}/crypto/sha1.c
  ${HOSTAP_SRC_BASE}/crypto/sha1-internal.c
  ${HOSTAP_SRC_BASE}/crypto/sha1-pbkdf2.c
  ${HOSTAP_SRC_BASE}/crypto/sha1-prf.c
  ${HOSTAP_SRC_BASE}/crypto/sha256.c
  ${HOSTAP_SRC_BASE}/crypto/sha256-internal.c
  ${HOSTAP_SRC_BASE}/crypto/sha256-prf.c
  ${HOSTAP_SRC_BASE}/crypto/sha384.c
  ${HOSTAP_SRC_BASE}/crypto/sha384-internal.c
  ${HOSTAP_SRC_BASE}/crypto/sha384-prf.c
  ${HOSTAP_SRC_BASE}/crypto/sha512.c
  ${HOSTAP_SRC_BASE}/crypto/sha512-internal.c
  ${HOSTAP_SRC_BASE}/crypto/rc4.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_NONE
  CONFIG_NO_WPA
  CONFIG_CRYPTO_INTERNAL
)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_WEP
  CONFIG_WEP
)

zephyr_library_sources_ifndef(CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_ENTERPRISE
  ${HOSTAP_SRC_BASE}/crypto/tls_none.c
)

zephyr_library_sources_ifndef(CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_NONE
  ${HOSTAP_SRC_BASE}/common/wpa_common.c
  ${HOSTAP_SRC_BASE}/rsn_supp/wpa.c
  ${HOSTAP_SRC_BASE}/rsn_supp/preauth.c
  ${HOSTAP_SRC_BASE}/rsn_supp/wpa_ie.c
)

zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_WPA3
  ${HOSTAP_SRC_BASE}/common/sae.c
  ${HOSTAP_SRC_BASE}/common/dragonfly.c

  ${HOSTAP_SRC_BASE}/crypto/dh_groups.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_WPA3
  CONFIG_SAE
  CONFIG_ECC
)

zephyr_library_include_directories_ifndef(CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_NONE
  ${CMAKE_SOURCE_DIR}
)

zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_P2P
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/p2p_supplicant.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/p2p_supplicant_sd.c
  ${HOSTAP_SRC_BASE}/p2p/p2p.c
  ${HOSTAP_SRC_BASE}/p2p/p2p_utils.c
  ${HOSTAP_SRC_BASE}/p2p/p2p_parse.c
  ${HOSTAP_SRC_BASE}/p2p/p2p_build.c
  ${HOSTAP_SRC_BASE}/p2p/p2p_go_neg.c
  ${HOSTAP_SRC_BASE}/p2p/p2p_sd.c
  ${HOSTAP_SRC_BASE}/p2p/p2p_pd.c
  ${HOSTAP_SRC_BASE}/p2p/p2p_invitation.c
  ${HOSTAP_SRC_BASE}/p2p/p2p_dev_disc.c
  ${HOSTAP_SRC_BASE}/p2p/p2p_group.c
  ${HOSTAP_SRC_BASE}/ap/p2p_hostapd.c
  ${HOSTAP_SRC_BASE}/common/gas.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/gas_query.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/offchannel.c
)
zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_WPS
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/wps_supplicant.c
  ${HOSTAP_SRC_BASE}/utils/uuid.c
  ${HOSTAP_SRC_BASE}/eap_peer/eap_wsc.c
  ${HOSTAP_SRC_BASE}/eap_common/eap_wsc_common.c
  ${HOSTAP_SRC_BASE}/wps/wps.c
  ${HOSTAP_SRC_BASE}/ap/wps_hostapd.c
  ${HOSTAP_SRC_BASE}/wps/wps_common.c
  ${HOSTAP_SRC_BASE}/wps/wps_attr_parse.c
  ${HOSTAP_SRC_BASE}/wps/wps_attr_build.c
  ${HOSTAP_SRC_BASE}/wps/wps_attr_process.c
  ${HOSTAP_SRC_BASE}/wps/wps_dev_attr.c
  ${HOSTAP_SRC_BASE}/wps/wps_enrollee.c
  ${HOSTAP_SRC_BASE}/wps/wps_registrar.c
  ${HOSTAP_SRC_BASE}/crypto/dh_groups.c
)

if(NOT CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_ALT)
# dh_group5 is only needed if we are not using mbedtls, as mbedtls provides
# its own definition
zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_WPS
  ${HOSTAP_SRC_BASE}/crypto/dh_group5.c
)
endif()

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_P2P
  CONFIG_P2P
  CONFIG_GAS
  CONFIG_OFFCHANNEL
)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_WPS
  CONFIG_WPS
  EAP_WSC
)

zephyr_library_sources_ifdef(CONFIG_WIFI_NM_HOSTAPD_WPS
  ${HOSTAP_SRC_BASE}/eap_server/eap_server_wsc.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_HOSTAPD_WPS
  EAP_SERVER_WSC
)

zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_ENTERPRISE
  ${HOSTAP_SRC_BASE}/eap_common/eap_common.c
  ${HOSTAP_SRC_BASE}/eap_peer/eap_tls_common.c

)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_ENTERPRISE
  IEEE8021X_EAPOL
  EAP_IKEv2
)

zephyr_library_sources_ifdef(CONFIG_EAP_TLS
  ${HOSTAP_SRC_BASE}/eap_peer/eap_tls.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_TLS
  EAP_TLS
)

zephyr_library_sources_ifdef(CONFIG_EAP_TTLS
  ${HOSTAP_SRC_BASE}/eap_peer/eap_ttls.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_TTLS
  EAP_TTLS
)

zephyr_library_sources_ifdef(CONFIG_EAP_PEAP
  ${HOSTAP_SRC_BASE}/eap_peer/eap_peap.c
  ${HOSTAP_SRC_BASE}/eap_common/eap_peap_common.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_PEAP
  EAP_PEAP
)

zephyr_library_sources_ifdef(CONFIG_EAP_MD5
  ${HOSTAP_SRC_BASE}/eap_peer/eap_md5.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_MD5
  EAP_MD5
)

zephyr_library_sources_ifdef(CONFIG_EAP_GTC
  ${HOSTAP_SRC_BASE}/eap_peer/eap_gtc.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_GTC
  EAP_GTC
)

zephyr_library_sources_ifdef(CONFIG_EAP_MSCHAPV2
  ${HOSTAP_SRC_BASE}/eap_peer/eap_mschapv2.c
  ${HOSTAP_SRC_BASE}/eap_peer/mschapv2.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_MSCHAPV2
  EAP_MSCHAPv2
)

if(CONFIG_EAP_TTLS OR CONFIG_EAP_MSCHAPV2 OR CONFIG_EAP_MD5)
  zephyr_library_sources(${HOSTAP_SRC_BASE}/eap_common/chap.c)
endif()

zephyr_library_sources_ifdef(CONFIG_EAP_LEAP
  ${HOSTAP_SRC_BASE}/eap_peer/eap_leap.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_LEAP
  EAP_LEAP
)

zephyr_library_sources_ifdef(CONFIG_EAP_PSK
  ${HOSTAP_SRC_BASE}/eap_peer/eap_psk.c
  ${HOSTAP_SRC_BASE}/eap_common/eap_psk_common.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_PSK
  EAP_PSK
)

zephyr_library_sources_ifdef(CONFIG_EAP_PAX
  ${HOSTAP_SRC_BASE}/eap_peer/eap_pax.c
  ${HOSTAP_SRC_BASE}/eap_common/eap_pax_common.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_PAX
  EAP_PAX
)

zephyr_library_sources_ifdef(CONFIG_EAP_SAKE
  ${HOSTAP_SRC_BASE}/eap_peer/eap_sake.c
  ${HOSTAP_SRC_BASE}/eap_common/eap_sake_common.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_SAKE
  EAP_SAKE
)

zephyr_library_sources_ifdef(CONFIG_EAP_GPSK
  ${HOSTAP_SRC_BASE}/eap_peer/eap_gpsk.c
  ${HOSTAP_SRC_BASE}/eap_common/eap_gpsk_common.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_GPSK
  EAP_GPSK
)

zephyr_library_sources_ifdef(CONFIG_EAP_PWD
  ${HOSTAP_SRC_BASE}/eap_peer/eap_pwd.c
  ${HOSTAP_SRC_BASE}/eap_common/eap_pwd_common.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_PWD
  EAP_PWD
)

zephyr_library_sources_ifdef(CONFIG_EAP_EKE
  ${HOSTAP_SRC_BASE}/eap_peer/eap_eke.c
  ${HOSTAP_SRC_BASE}/eap_common/eap_eke_common.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_EKE
  EAP_EKE
)

zephyr_library_sources_ifdef(CONFIG_EAP_IKEV2
  ${HOSTAP_SRC_BASE}/eap_peer/eap_ikev2.c
  ${HOSTAP_SRC_BASE}/eap_peer/ikev2.c
  ${HOSTAP_SRC_BASE}/eap_common/eap_ikev2_common.c
  ${HOSTAP_SRC_BASE}/eap_common/ikev2_common.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_IKEV2
  EAP_IKEV2
)

if (CONFIG_EAP_SIM OR CONFIG_EAP_AKA)
  zephyr_library_sources(${HOSTAP_SRC_BASE}/eap_common/eap_sim_common.c)
endif()

zephyr_library_sources_ifdef(CONFIG_EAP_SIM
  ${HOSTAP_SRC_BASE}/eap_peer/eap_sim.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_SIM
  EAP_SIM
)

zephyr_library_sources_ifdef(CONFIG_EAP_AKA
  ${HOSTAP_SRC_BASE}/eap_peer/eap_aka.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_AKA
  EAP_AKA
)

# Needs TLS1.3 and SESSION_TICKETS
zephyr_library_sources_ifdef(CONFIG_EAP_FAST
  ${HOSTAP_SRC_BASE}/eap_peer/eap_fast.c
  ${HOSTAP_SRC_BASE}/eap_peer/eap_fast_pac.c
  ${HOSTAP_SRC_BASE}/eap_common/eap_fast_common.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_FAST
  EAP_FAST
)

# Needs TLS1.3 and SESSION_TICKETS
zephyr_library_sources_ifdef(CONFIG_EAP_FAST
  ${HOSTAP_SRC_BASE}/eap_peer/eap_fast.c
  ${HOSTAP_SRC_BASE}/eap_peer/eap_fast_pac.c
  ${HOSTAP_SRC_BASE}/eap_common/eap_fast_common.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_FAST
  EAP_FAST
)

zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_EAPOL
  ${HOSTAP_SRC_BASE}/eapol_supp/eapol_supp_sm.c
  ${HOSTAP_SRC_BASE}/eap_peer/eap.c
  ${HOSTAP_SRC_BASE}/eap_peer/eap_methods.c
  ${HOSTAP_SRC_BASE}/eap_common/eap_common.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_EAPOL
  IEEE8021X_EAPOL
)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_NW_SEL_RELIABILITY
  CONFIG_NW_SEL_RELIABILITY
)

zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_DPP
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/dpp_supplicant.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/offchannel.c
  ${WIFI_NM_WPA_SUPPLICANT_BASE}/gas_query.c

  ${HOSTAP_SRC_BASE}/ap/dpp_hostapd.c
  ${HOSTAP_SRC_BASE}/ap/gas_query_ap.c
  ${HOSTAP_SRC_BASE}/ap/gas_serv.c

  ${HOSTAP_SRC_BASE}/common/dpp_tcp.c
  ${HOSTAP_SRC_BASE}/common/dpp.c
  ${HOSTAP_SRC_BASE}/common/dpp_pkex.c
  ${HOSTAP_SRC_BASE}/common/dpp_crypto.c
  ${HOSTAP_SRC_BASE}/common/dpp_auth.c
  ${HOSTAP_SRC_BASE}/common/dpp_reconfig.c
  ${HOSTAP_SRC_BASE}/common/gas_server.c
  ${HOSTAP_SRC_BASE}/common/gas.c
  ${HOSTAP_SRC_BASE}/common/dpp_backup.c

  ${HOSTAP_SRC_BASE}/crypto/aes-siv.c

  ${HOSTAP_SRC_BASE}/utils/json.c
  ${HOSTAP_SRC_BASE}/utils/ip_addr.c

  ${HOSTAP_SRC_BASE}/tls/asn1.c
)

zephyr_library_sources_ifdef(CONFIG_WIFI_NM_HOSTAPD_CRYPTO_ENTERPRISE
  ${HOSTAP_SRC_BASE}/eap_server/eap_server_tls_common.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_WIFI_NM_HOSTAPD_CRYPTO_ENTERPRISE
  WIFI_NM_HOSTAPD_CRYPTO_ENTERPRISE
  EAP_TLS_FUNCS
  EAP_SERVER
)

zephyr_library_sources_ifdef(CONFIG_EAP_SERVER_IDENTITY
  ${HOSTAP_SRC_BASE}/eap_server/eap_server_identity.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_SERVER_IDENTITY
  EAP_SERVER_IDENTITY
)


zephyr_library_sources_ifdef(CONFIG_EAP_SERVER_TLS
  ${HOSTAP_SRC_BASE}/eap_server/eap_server_tls.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_SERVER_TLS
  EAP_SERVER_TLS
)

zephyr_library_sources_ifdef(CONFIG_EAP_SERVER_MD5
  ${HOSTAP_SRC_BASE}/eap_server/eap_server_md5.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_SERVER_MD5
  EAP_SERVER_MD5
)

zephyr_library_sources_ifdef(CONFIG_EAP_SERVER_MSCHAPV2
  ${HOSTAP_SRC_BASE}/eap_server/eap_server_mschapv2.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_SERVER_MSCHAPV2
  EAP_SERVER_MSCHAPV2
)

zephyr_library_sources_ifdef(CONFIG_EAP_SERVER_PEAP
  ${HOSTAP_SRC_BASE}/eap_server/eap_server_peap.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_SERVER_PEAP
  EAP_SERVER_PEAP
)

zephyr_library_sources_ifdef(CONFIG_EAP_SERVER_GTC
  ${HOSTAP_SRC_BASE}/eap_server/eap_server_gtc.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_SERVER_GTC
  EAP_SERVER_GTC
)

zephyr_library_sources_ifdef(CONFIG_EAP_SERVER_TTLS
  ${HOSTAP_SRC_BASE}/eap_server/eap_server_ttls.c
)

zephyr_library_compile_definitions_ifdef(CONFIG_EAP_SERVER_TTLS
  EAP_SERVER_TTLS
)

if(CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_ALT)
zephyr_include_directories(
  ${HOSTAP_BASE}/port/mbedtls
)

zephyr_library_sources(
  ${HOSTAP_SRC_BASE}/crypto/crypto_mbedtls_alt.c
  ${HOSTAP_SRC_BASE}/crypto/rc4.c
)

zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_MBEDTLS_PSA
  ${HOSTAP_BASE}/port/mbedtls/supp_psa_api.c
)

zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_ENTERPRISE
  ${HOSTAP_SRC_BASE}/crypto/ms_funcs.c
  ${HOSTAP_SRC_BASE}/crypto/aes-eax.c
  ${HOSTAP_SRC_BASE}/crypto/md4-internal.c
  ${HOSTAP_SRC_BASE}/crypto/sha1-internal.c
  ${HOSTAP_SRC_BASE}/crypto/fips_prf_internal.c
  ${HOSTAP_SRC_BASE}/crypto/milenage.c
  ${HOSTAP_SRC_BASE}/crypto/tls_mbedtls_alt.c
)

zephyr_library_sources_ifdef(CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_TEST
  ${HOSTAP_SRC_BASE}/crypto/crypto_module_tests.c
  ${HOSTAP_SRC_BASE}/crypto/fips_prf_internal.c
  ${HOSTAP_SRC_BASE}/crypto/sha1-internal.c
  ${HOSTAP_SRC_BASE}/crypto/sha1-tlsprf.c
)
endif()

zephyr_library_link_libraries_ifndef(CONFIG_WIFI_NM_WPA_SUPPLICANT_CRYPTO_NONE
  mbedTLS)

if(CONFIG_SAE_PWE_EARLY_EXIT)
  message(WARNING "CONFIG_SAE_PWE_EARLY_EXIT is enabled, "
  "this is not secure and is a workaround for low resource systems, "
  "please use it carefully and do not use it production.")
endif()

endif()
