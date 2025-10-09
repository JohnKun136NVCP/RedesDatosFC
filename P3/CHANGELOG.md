\[2.2.0] - 2025-09-23

Added

* `GET /formTemplate/{idFormTemplate}` ahora puede incluir un bloque opcional `dynamic` por **campo** (y por **opción**) con: `triggerAction`, `dependsOn`, `valueMap`, `calculateFnName`, `dataSource`.
* `POST /formTemplate/{idFormTemplate}/evaluate` para evaluar en **lote** reglas dinámicas (visibilidad, habilitado, opciones, valores calculados) dado un `context.answers`.
* `POST /field/{idField}/options/resolve` (o `/templates/{templateId}/fields/{fieldId}/resolve`) para resolver **efectos** dinámicos puntuales (`updateOptions`, `calculatedValue`, `show/hide`, `enable/disable`, `copyValue`).
* `GET /field/{idField}/options` con `q`, `page`, `size`, `active`, `sourceValue` para **paginación/filtrado** y dependencias de opciones.
* `HEAD /formTemplate/{idFormTemplate}` con `ETag/Last-Modified` (y `X-Template-Version`) para **caché** de plantillas.

Changed

* **FormController** valida en servidor antes de persistir:
  `required` solo si el campo está **visible y habilitado**; limpieza/rechazo de valores de campos ocultos; verificación de que las selecciones pertenezcan al conjunto permitido tras `updateOptions`; política clara para `calculatedValue` (el servidor puede **recalcular/sobrescribir**).
* **FieldOptionController**: preferir `active=false` para inactivar en lugar de `position = -1` (se mantiene compatibilidad con `-1`).
* **FormTemplateController**: estandariza `DELETE /formTemplate/{id}` (mantener temporalmente `/.../delete` por compatibilidad).
* **Errores** estandarizados con referencia por `fieldId`: `required-when-visible`, `value-not-in-options`, `calculation-mismatch`, `field-hidden-must-be-empty`.
* Respuestas de plantilla y opciones con **headers de caché** coherentes (uso de `ETag` / `Last-Modified`).
