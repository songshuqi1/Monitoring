import api from '../api/index.js'

const COLLECTED_SOURCES = new Set(['OPCUA', 'MODBUS', 'UDP', 'SDC'])

/**
 * Every collected variable is an automatic command resource.  The variable
 * keeps the resource id and protocol binding created by the collector, so the
 * write always returns over the same communication resource without asking the
 * user to enter an address a second time.
 */
export async function writeCollectedVariable(variableId, value) {
  const numericValue = Number(value)
  if (!Number.isFinite(numericValue)) throw new Error('请输入有效的有限数值')
  const variableResult = await api.getVariable(variableId)
  const rawVariable = variableResult?.data
  let variable = rawVariable
  if (typeof rawVariable === 'string') {
    try {
      variable = JSON.parse(rawVariable)
    } catch {
      throw new Error('变量详情返回格式无效，请刷新运行时后重试')
    }
  }
  if (!variable) throw new Error('变量不存在或已被删除')
  if (!COLLECTED_SOURCES.has(String(variable.source || '').toUpperCase())) {
    throw new Error('该变量不是通信资源自动采集的可下发变量')
  }

  let response
  try {
    response = await api.writeVariable(variableId, numericValue)
  } catch (error) {
    throw new Error(error?.response?.data?.message || error?.message || '控制器未确认写入')
  }
  const status = String(response?.data?.writeStatus || '').toUpperCase()
  if (COLLECTED_SOURCES.has(String(variable.source || '').toUpperCase()) && status !== 'SUCCESS') {
    throw new Error(response?.data?.message || '控制器未确认写入，请检查通信资源与可写地址')
  }
  return response
}
